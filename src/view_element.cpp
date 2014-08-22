/////////////////////////////////////////////////////////////////////////////
// Name:        musrc_element.cpp
// Author:      Laurent Pugin and Chris Niven
// Created:     2011
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////


#include "view.h"

//----------------------------------------------------------------------------

#include <assert.h>
#include <sstream>
#include <typeinfo>

//----------------------------------------------------------------------------

#include "vrv.h"
#include "app.h"
#include "barline.h"
#include "beam.h"
#include "clef.h"
#include "doc.h"
#include "keysig.h"
#include "layerelement.h"
#include "leipzigbbox.h"
#include "measure.h"
#include "mensur.h"
#include "metersig.h"
#include "mrest.h"
#include "multirest.h"
#include "note.h"
#include "rest.h"
#include "symbol.h"
#include "staff.h"
#include "syl.h"
#include "system.h"
#include "slur.h"
#include "tie.h"
#include "tuplet.h"
#include "verse.h"

namespace vrv {

//----------------------------------------------------------------------------
// View - LayerElement
//----------------------------------------------------------------------------

int View::s_drawingLigX[2], View::s_drawingLigY[2];	// pour garder coord. des ligatures    
bool View::s_drawingLigObliqua = false;	// marque le 1e passage pour une oblique

void View::DrawElement( DeviceContext *dc, LayerElement *element, Layer *layer, Measure *measure, Staff *staff )
{
    assert(layer); // Pointer to layer cannot be NULL"
    assert(staff); // Pointer to staff cannot be NULL"
    
    int previousColor = m_currentColour;

    if (element == m_currentElement) {
		m_currentColour = AxRED;
    }
    else {
        m_currentColour = AxBLACK;
    }
    
    // Here we set the appropriate x value to be used for drawing
    // With Raw documents, we use m_drawingXRel that is calculated by the layout algorithm
    // With Transcription documents, we use the m_xAbs
    if ( element->m_xAbs == VRV_UNSET ) {
        assert( m_doc->GetType() == Raw );
        element->SetDrawingX( element->GetXRel() + measure->GetDrawingX() );
        element->SetDrawingY( staff->GetDrawingY() );
    }
    else
    {
        assert( m_doc->GetType() == Transcription );
        element->SetDrawingX( element->m_xAbs );
        element->SetDrawingY( staff->GetDrawingY() );
    }
    
    if (dynamic_cast<Barline*>(element)) {
        DrawBarline(dc, element, layer, staff);
    }
    else if (dynamic_cast<Beam*>(element)) {
        DrawBeamElement(dc, element, layer, measure, staff);
    }
    else if (dynamic_cast<Clef*>(element)) {
        DrawClef(dc, element, layer, staff);
    }
    else if (dynamic_cast<KeySig*>(element)) {
        DrawKeySig(dc, element, layer, staff);
    }
    else if (dynamic_cast<Mensur*>(element)) {
        DrawMensur(dc, element, layer, staff);
    }
    else if (dynamic_cast<MeterSig*>(element)) {
        DrawMeterSig(dc, element, layer, staff);
    }
    else if (dynamic_cast<MRest*>(element)) {
        DrawMRest(dc, element, layer, staff, measure);
    }
    else if (dynamic_cast<MultiRest*>(element)) {
        DrawMultiRest(dc, element, layer, staff, measure);
    }
    else if (dynamic_cast<Note*>(element)) {
        DrawDurationElement(dc, element, layer, staff);
    }
    else if (dynamic_cast<Rest*>(element)) {
        DrawDurationElement(dc, element, layer, staff);
    }
    else if (dynamic_cast<Symbol*>(element)) {
        DrawSymbol(dc, element, layer, staff);
    }
    else if (dynamic_cast<Tie*>(element)) {
        DrawTie(dc, element, layer, staff, measure);
    }
    else if (dynamic_cast<Slur*>(element)) {
        DrawTie(dc, element, layer, staff, measure);
    }
    else if (dynamic_cast<Tuplet*>(element)) {
        DrawTupletElement(dc, element, layer, measure, staff);
    }
    else if (dynamic_cast<LayerApp*>(element)) {
        DrawLayerApp(dc, element, layer, measure, staff);
    }
    
    m_currentColour = previousColor;
}

void View::DrawDurationElement( DeviceContext *dc, LayerElement *element, Layer *layer, Staff *staff )
{
    assert(layer); // Pointer to layer cannot be NULL"
    assert(staff); // Pointer to staff cannot be NULL"

    DurationInterface *durElement = dynamic_cast<DurationInterface*>(element);
	if ( !durElement )
		return;
        
	
    if (dynamic_cast<Note*>(element)) 
    {
        Note *note = dynamic_cast<Note*>(element);
        int oct = note->m_oct - 4;
        
        element->SetDrawingY( element->GetDrawingY() + CalculatePitchPosY( staff, note->m_pname, layer->GetClefOffset( element ), oct ) );
        dc->StartGraphic( element, "note", element->GetUuid() );
        DrawNote(dc, element, layer, staff);
        dc->EndGraphic(element, this );
	}
    else if (dynamic_cast<Rest*>(element)) {
        Rest *rest = dynamic_cast<Rest*>(element);
        int oct = rest->m_oct - 4;
        
        // Automatically calculate rest position, if so requested
        if (rest->m_pname == REST_AUTO)
            element->SetDrawingY( element->GetDrawingY() + CalculateRestPosY( staff, rest->m_dur) );
        else
            element->SetDrawingY( element->GetDrawingY() + CalculatePitchPosY( staff, rest->m_pname, layer->GetClefOffset( element ), oct) );
		
        dc->StartGraphic( element, "rest", element->GetUuid() );
        DrawRest( dc, element, layer, staff );
        dc->EndGraphic(element, this );
	}
	
	return;
}

void View::DrawBeamElement(DeviceContext *dc, LayerElement *element, Layer *layer, Measure *measure, Staff *staff) {
    
    assert(layer); // Pointer to layer cannot be NULL"
    assert(staff); // Pointer to staff cannot be NULL"
    
    Beam *beam = dynamic_cast<Beam*>(element);

    dc->StartGraphic( element, "beam", element->GetUuid() );
    
    for (unsigned int i = 0; i < beam->m_children.size(); i++) {
        if ( dynamic_cast<LayerElement*>(beam->m_children[i]) ) {
            LayerElement *element = dynamic_cast<LayerElement*>(beam->m_children[i]);
            DrawElement(dc, element, layer, measure, staff);
        }
    }
    
    // Add to the list of postponed element 
    layer->AddToDrawingList( beam );
    
    dc->EndGraphic(element, this );
}

void View::DrawTupletElement(DeviceContext *dc, LayerElement *element, Layer *layer, Measure *measure, Staff *staff) {
    
    assert(layer); // Pointer to layer cannot be NULL"
    assert(staff); // Pointer to staff cannot be NULL"
    
    Tuplet *tuplet = dynamic_cast<Tuplet*>(element);
    
    dc->StartGraphic( element, "tuplet", element->GetUuid() );
    
    // Draw the inner elements
    for (unsigned int i = 0; i < tuplet->m_children.size(); i++) {
        if ( dynamic_cast<LayerElement*>(tuplet->m_children[i]) ) {
            LayerElement *element = dynamic_cast<LayerElement*>(tuplet->m_children[i]);
            DrawElement(dc, element, layer, measure, staff);
        }
    }
    
    // Add to the list of postponed element
    layer->AddToDrawingList( tuplet );
    
    dc->EndGraphic(element, this );
}

// dessine la note en a,b+by. Calcule et dessine lignes addit. avec by=m_drawingY
// b = decalage en fonction oct., clef, a partir du curseur; by = pos. curs.
// Accords: note doit connaitre le x non modifie par accord(), la fin de 
// l'accord (ptr_n->fchord), la valeur y extreme opposee au sommet de la
// queue: le ptr *testchord extern peut garder le x et l'y.


void View::DrawNote ( DeviceContext *dc, LayerElement *element, Layer *layer, Staff *staff)
{
    assert(layer); // Pointer to layer cannot be NULL"
    assert(staff); // Pointer to staff cannot be NULL"
    assert(dynamic_cast<Note*>(element)); // Element must be a Note"
    
    Note *note = dynamic_cast<Note*>(element);
    
    bool inBeam = false;
    
    // Get the immadiate parent of the note
    // to see if beamed or not
    Object *note_parent = note->GetFirstParent(&typeid(Beam));
    
    // This note is beamed and cue sized
    if (note_parent != NULL) {
        if (note->m_cueSize == true) {
            // Get the Parent of the parent
            // we want to see if we are in a group of
            // beamed cue notes
            Beam *b = dynamic_cast<Beam*>(note_parent);
            if (b->GetListIndex(note) > -1) {
                inBeam = true;
            }
                        
        } else {
            // the note is just in a beam
            inBeam = true;
        }
    }
    
	int staffSize = staff->staffSize;

	//	int horphyspoint=h_pnt;
	//int b = element->m_drawingY;
	int i, valdec, fontNo, ledge, queueCentre;
	int x1, x2, y2, espac7, decval, vertical;
	int formval = 0;	// pour permettre dessiner colorations avec dÇcalage de val
	int rayon, milieu = 0;

	int xn = element->GetDrawingX(), xl = element->GetDrawingX();
	int bby = staff->GetDrawingY();  // bby= y sommet portee
	int ynn = element->GetDrawingY();
	//static int ynn_chrd;

	xn += note->m_hOffset;
	//val=note->m_dur;
	formval = ((note->GetColored()==BOOLEAN_true) && note->m_dur > DUR_1) ? (note->m_dur+1) : note->m_dur;
	queueCentre = 0;


	rayon = m_doc->m_drawingNoteRadius[staffSize][note->m_cueSize];

	if (note->m_dur > DUR_1 || (note->m_dur == DUR_1 && staff->notAnc))	// annuler provisoirement la modif. des lignes addit.
		ledge = m_doc->m_drawingLedgerLine[staffSize][note->m_cueSize];
	else
	{	
        ledge= m_doc->m_drawingLedgerLine[staffSize][2];
		rayon += rayon/3;
	}

	x1 = xn-rayon;	// position d'appel du caractäre et de la queue gauche
    xl = xn;

	// permettre d'inverser le cotÇ de la tete de note avec flag lat
	/*if (this->lat && !this->chord)
	{	if (this->queue)
		{	x1 = xn + rayon;
			xl = xn + rayon * 2;
		}
		else
		{	x1 = xn - rayon * 3;
			xl = xn - rayon * 2;
		}
	}*/ // ax2 - not support of lat

	DrawLedgerLines( dc, ynn, bby, xl, ledge, staffSize);	// dessin lignes additionnelles

	if (note->m_dur == DUR_LG || note->m_dur == DUR_BR || ((note->GetLig()!=LIGATURE_NONE) && note->m_dur == DUR_1))	// dessin carrees
	{
		DrawLigature ( dc, ynn, element, layer, staff);
 	}
	else if (note->m_dur==DUR_1)
	{	
        if (note->GetColored()==BOOLEAN_true) // && !note->m_ligObliqua) // in WG, use of obliq for coloration?
			fontNo = LEIPZIG_HEAD_WHOLE_FILLED;
		else
			fontNo = LEIPZIG_HEAD_WHOLE;

		DrawLeipzigFont( dc, x1, ynn, fontNo, staff, note->m_cueSize );
		decval = ynn;
	}
	else
	{	
        if ((note->GetColored()==BOOLEAN_true) || formval == DUR_2)
			fontNo = LEIPZIG_HEAD_HALF;
		else
			fontNo = LEIPZIG_HEAD_QUARTER;

		DrawLeipzigFont( dc,x1, ynn, fontNo, staff, note->m_cueSize );

		milieu = bby - m_doc->m_drawingInterl[staffSize]*2;

// test ligne mediane pour direction queues: notation mesuree, milieu queue haut
		if (staff->notAnc)
			milieu +=  m_doc->m_drawingHalfInterl[staffSize];

		//if (note->m_chord) { /*** && this == testchord)***/
		//	ynn_chrd = ynn;
        //}
		if (inBeam && formval > DUR_4)
        {
            // no stem
		}
		//else if (note->m_headshape != SANSQUEUE && (!note->m_chord || (note->m_chord==CHORD_TERMINAL))) {
        else {
            /*if (note->m_chord==CHORD_TERMINAL) {
				up = testchord->obj.not.haste;
				xn = testchord->m_drawingX;
			}
			else */
            {
				//***up = this->q_auto ? ((ynn < milieu)? ON :OFF):this->queue;
				// ENZ
				note->d_stemDir = (ynn > milieu) ? STEMDIRECTION_down : STEMDIRECTION_up;
            }
			
			// ENZ
			if ( note->HasStemDir() ) {
				//up = (up == ON) ? OFF : ON;
                note->d_stemDir = note->GetStemDir();
            }
			
			espac7 = note->m_cueSize ? ( m_doc->m_drawingHalfInterl[staffSize]*5) : ( m_doc->m_drawingHalfInterl[staffSize]*7);
			vertical = note->m_cueSize ?  m_doc->m_drawingHalfInterl[staffSize] :  m_doc->m_drawingInterl[staffSize];
			decval = vertical * (valdec = formval-DUR_8);
			
			/***if (this->existDebord)	// queue longueur manuelle
				traiteQueue (&espac7, this);***/

			// diminuer le rayon de la moitie de l'epaisseur du trait de queue
			rayon -= (m_doc->m_env.m_stemWidth) / 2;

			if (note->d_stemDir == STEMDIRECTION_down) {	// si queue vers le bas (a gauche)
				espac7 = -espac7;
				decval = -decval;
				rayon = -rayon;
			}

			y2 = ((formval>DUR_8) ? (ynn + espac7 + decval) : (ynn + espac7));

			/*if ((note->m_chord==CHORD_INITIAL) || (note->m_chord==CHORD_MEDIAL)) {
				ynn = ynn_chrd;
            }*/

			/***if (this->q_auto)
			{	this->queue = (up > 0);
				if ( (y2 >= milieu && ynn > milieu) || (y2 <= milieu && ynn < milieu) )
				// note et queue du meme cote par rapport au centre de la portee
				{	y2 = milieu;
					queueCentre = 1;
				}
			}***/

			if (staff->notAnc)
				rayon = 0;
			x2 = xn + rayon;

			if (note->d_stemDir == STEMDIRECTION_up)
			{
				if (formval > DUR_8 && !queueCentre)
				// Le 24 Septembre 1993. Correction esthetique pour rapprocher tailles 
				//   des DUR_8 et DUR_16 (longeur de queues trop inegales).
					y2 -= m_doc->m_drawingHalfInterl[staffSize];
				decval = y2;
				if (staff->notAnc)
					DrawVerticalLine ( dc,y2,(int)(ynn + m_doc->m_drawingHalfInterl[staffSize]),x2, m_doc->m_env.m_stemWidth );//queue en descendant
				else
					DrawVerticalLine ( dc,y2,(int)(ynn+ m_doc->m_drawingVerticalUnit2[staffSize]),x2 - (m_doc->m_env.m_stemWidth / 2), m_doc->m_env.m_stemWidth );//queue en descendant
                
                element->m_drawingStemStart.x = element->m_drawingStemEnd.x = x2 - (m_doc->m_env.m_stemWidth / 2);
                element->m_drawingStemEnd.y = y2;
                element->m_drawingStemStart.y = (int)(ynn+ m_doc->m_drawingVerticalUnit2[staffSize]);
                element->m_drawingStemDir = true;
                
				if (formval > DUR_4)
				{
                    y2 += m_doc->m_env.m_stemWidth / 2; // ENZO correction empirique...
					DrawLeipzigFont( dc,x2,y2,LEIPZIG_STEM_FLAG_UP, staff, note->m_cueSize );
					for (i=0; i < valdec; i++)
						DrawLeipzigFont( dc,x2,y2-=vertical,LEIPZIG_STEM_FLAG_UP, staff, note->m_cueSize );
				}
			}
			else
			{                
				if (formval > DUR_8 && !queueCentre)
				// Le 24 Septembre 1993. Correction esthetique pour rapprocher tailles 
				//   des DUR_8 et DUR_16 (longeur de queues trop inegales).
					y2 += m_doc->m_drawingHalfInterl[staffSize];
				decval = y2;

				if (staff->notAnc)
					DrawVerticalLine ( dc,y2,ynn- m_doc->m_drawingHalfInterl[staffSize],x2 - (m_doc->m_env.m_stemWidth / 2), m_doc->m_env.m_stemWidth );//queue en descendant
				else
					DrawVerticalLine ( dc,y2,(int)(ynn- m_doc->m_drawingVerticalUnit2[staffSize]),x2 - (m_doc->m_env.m_stemWidth / 2), m_doc->m_env.m_stemWidth );	// queue en montant

                element->m_drawingStemStart.x = element->m_drawingStemEnd.x = x2 - (m_doc->m_env.m_stemWidth / 2);
                element->m_drawingStemStart.y = (int)(ynn- m_doc->m_drawingVerticalUnit2[staffSize]);
                element->m_drawingStemEnd.y = y2;
                element->m_drawingStemDir = false;
                
				// ENZ
				// decalage du crochet vers la gauche
				// changement dans la fonte Leipzig 4.3 ‡ cause de problemes d'affichage
				// en deÁ‡ de 0 avec la notation ancienne
				// dans la fonte les crochets ont ete decales de 164 vers la droite
				int cr_offset = m_doc->m_drawingNoteRadius[staffSize][note->m_cueSize]  + (m_doc->m_env.m_stemWidth / 2);
				if (formval > DUR_4)
				{
                    y2 -= m_doc->m_env.m_stemWidth / 2; // ENZO correction empirique...
					DrawLeipzigFont( dc,x2 - cr_offset,y2,LEIPZIG_STEM_FLAG_DOWN , staff, note->m_cueSize );
					for (i=0; i < valdec; i++)
						DrawLeipzigFont( dc,x2  - cr_offset,y2+=vertical,LEIPZIG_STEM_FLAG_DOWN, staff, 
									 note->m_cueSize );
				}
			}
            if (note->m_cueSize && note->m_acciaccatura)
                DrawAcciaccaturaSlash(dc, element);
		}	// fin de dessin queues et crochets

	}
    
	DrawLedgerLines( dc, ynn, bby, xl, ledge, staffSize);
    
	if (note->m_accid) // && !this->accInvis) // ax2 no support invisible accidental yet
	{
		//if (note->m_chord)
        //    {}/***x1 = x_acc_chrd (this,0);***/
		//else
			x1 -= 1.5 * m_doc->m_drawingAccidWidth[staffSize][note->m_cueSize];
		
        Symbol accid( SYMBOL_ACCID );
        accid.m_oct = note->m_oct;
        accid.m_pname = note->m_pname;
		accid.m_accid = note->m_accid;
        accid.SetDrawingX( x1 );
        accid.SetDrawingY( staff->GetDrawingY() );
        DrawSymbol( dc, &accid, layer, staff, element ); // ax2
	}
	if (0) //if (note->m_chord)
	{	
        /***x2 = testchord->m_drawingX + m_doc->m_drawingStep2;
		if (this->haste)
		{	if (this->lat || (this->ptr_fe && this->ptr_fe->type==NOTE && this->ptr_fe->obj.not.lat)
				|| (this->ptr_pe && element->m_drawingX==this->ptr_pe->m_drawingX && this->ptr_pe->type==NOTE && this->ptr_pe->obj.not.lat
					&& this->dec_y - this->ptr_pe->dec_y < m_doc->m_drawingInterl[staffSize]
					&& 0 != ((int)b % (int)m_doc->m_drawingInterl[staffSize]))
				)
				x2 += m_doc->m_drawingNoteRadius[staffSize][dimin] * 2;
		}*///
	}
	else
	{	if (note->m_dur < DUR_2 || (note->m_dur > DUR_8 && !inBeam && (note->d_stemDir == STEMDIRECTION_up)))
			x2 = xn + m_doc->m_drawingStep1*7/2;
		else
			x2 = xn + m_doc->m_drawingStep1*5/2;

		//if (this->lat) // ax2 - no support of note head flip
        //    x2 += rayon*2;
	}

	if (note->m_dots) // && (!this->pointInvisible)) // ax2 - no support of invisible dots yet
	{
		DrawDots( dc, x2, ynn, note->m_dots, staff );
	}
/*
	if (this->stacc && (!this->rel || !this->queue_lig))
	{
		if (!this->chord || ((!this->queue_lig && this == testchord) || (this->queue_lig && this->fchord )))
		{
			if (val > DUR_BR)
			{	if  (!this->queue_lig)
				{	if ((this->queue && !this->chord) || (this->chord && this->haste))
					{	b -= m_doc->m_drawingInterl[staffSize];
						decval = -m_doc->m_drawingInterl[staffSize];
					}
					else
					{	b += m_doc->m_drawingInterl[staffSize];
						decval = 0;
					}

				}
				else	// tous les cas inversÇs par queue_lig
				{	b = decval-staff->m_drawingY;
	
					if ((!this->queue && !this->chord) || (this->chord && !this->haste))
					{	b -= m_doc->m_drawingInterl[staffSize];
						decval = -1;
						if (val <= DUR_1)
							decval = -m_doc->m_drawingInterl[staffSize];

					}
					else
					{	b += m_doc->m_drawingHalfInterl[staffSize];
						decval = 0;
						if (val <= DUR_1)
							b += m_doc->m_drawingHalfInterl[staffSize];
					}
				}

			}
			else
				return;
			putStacc (hdc,xn,b,decval,this->typStac);
		}
	}
*/
	
	//temp debug code
//	m_currentColour = wxCYAN;
//	DrawFullRectangle(dc, element->m_drawingX - 3, ynn - 3, element->m_drawingX + 3, ynn + 3);
//	LogDebug("xrel: %d, ynn: %d", element->m_drawingX, ynn);
//	m_currentColour = wxBLACK;
	//temp debug code
    
    
    // Add the ties to the postponed drawing list
    if ( note->GetTieAttrInitial() ) {
        // normally, we add the tie from the terminal note,
        // however, when the notes are not on the same system (or page),
        // we need to draw them twice. For this reason, we look if the
        // parent system is the same or not. If not, we also add to the list
        // the tie from the inital note
        Note *noteTerminal = note->GetTieAttrInitial()->GetSecondNote();
        if ( noteTerminal ) {
            System *parentSystem1 = dynamic_cast<System*>( note->GetFirstParent( &typeid(System) ) );
            System *parentSystem2 = dynamic_cast<System*>( noteTerminal->GetFirstParent( &typeid(System) ) );
            if ( (parentSystem1 != parentSystem2) && parentSystem1 ) {
                layer->AddToDrawingList( note->GetTieAttrInitial() );
            }
        }
    }
    if ( note->GetTieAttrTerminal() ) {
        layer->AddToDrawingList( note->GetTieAttrTerminal() );
    }
    
    // same for slurs
    if ( note->GetSlurAttrInitial() ) {
        Note *noteTerminal = note->GetSlurAttrInitial()->GetSecondNote();
        if ( noteTerminal ) {
            System *parentSystem1 = dynamic_cast<System*>( note->GetFirstParent( &typeid(System) ) );
            System *parentSystem2 = dynamic_cast<System*>( noteTerminal->GetFirstParent( &typeid(System) ) );
            if ( (parentSystem1 != parentSystem2) && parentSystem1 ) {
                layer->AddToDrawingList( note->GetSlurAttrInitial() );
            }
        }
    }
    if ( note->GetSlurAttrTerminal() ) {
        layer->AddToDrawingList( note->GetSlurAttrTerminal() );
    }

	if (note->GetChildCount( &typeid(Verse) ) ) {
        DrawVerse(dc, dynamic_cast<Verse*>( note->GetFirst( &typeid(Verse) ) ), note, layer, staff );
    }

    if (note->m_fermata)
        DrawFermata(dc, element, staff);

    if (note->m_embellishment == EMB_TRILL)
        DrawTrill(dc, element, staff);
    
	return;
}


void View::DrawRest ( DeviceContext *dc, LayerElement *element, Layer *layer, Staff *staff )
{	
    assert(layer); // Pointer to layer cannot be NULL
    assert(staff); // Pointer to staff cannot be NULL
    assert(dynamic_cast<Rest*>(element)); // Element must be a Rest
        
    Rest *rest = dynamic_cast<Rest*>(element);

	int formval = rest->m_dur;
	int x = element->GetDrawingX() + rest->m_hOffset;
    int y = element->GetDrawingY();
    
    // Temporary fix for rest within tuplet because drawing tuplet requires m_drawingStemXXX to be set
    element->m_drawingStemStart.x = element->m_drawingStemEnd.x = element->GetDrawingX() - (m_doc->m_env.m_stemWidth / 2);
    element->m_drawingStemEnd.y = element->GetDrawingY();
    element->m_drawingStemStart.y = element->GetDrawingY();

    if (formval > DUR_2)
    {
		x -= m_doc->m_drawingNoteRadius[staff->staffSize][rest->m_cueSize];
    }

    switch (formval)
    {	
        case DUR_LG: DrawLongRest ( dc, x, y, staff); break;
        case DUR_BR: DrawBreveRest( dc, x, y, staff); break;
        case DUR_1:
        case DUR_2: DrawWholeRest ( dc, x, y, formval, rest->m_dots, rest->m_cueSize, staff); break;
        default: DrawQuarterRest( dc, x, y, formval, rest->m_dots, rest->m_cueSize, staff);
    }
    
    if(rest->m_fermata)
        DrawFermata(dc, element, staff);
    
	return;
}


void View::DrawLedgerLines( DeviceContext *dc, int y_n, int y_p, int xn, unsigned int smaller, int staffSize)
{
	int yn, ynt, yh, yb, test, v_decal = m_doc->m_drawingInterl[staffSize];
	int dist, xng, xnd;
	register int i;


	yh = y_p + m_doc->m_drawingHalfInterl[staffSize];
    yb = y_p - m_doc->m_drawingStaffSize[staffSize]- m_doc->m_drawingHalfInterl[staffSize];

	if (!is_in(y_n,yh,yb))                           // note hors-portee?
	{
		xng = xn - smaller;
		xnd = xn + smaller;

		dist = ((y_n > yh) ? (y_n - y_p) : y_p - m_doc->m_drawingStaffSize[staffSize] - y_n);
  		ynt = ((dist % m_doc->m_drawingInterl[staffSize] > 0) ? (dist - m_doc->m_drawingHalfInterl[staffSize]) : dist);
		test = ynt/ m_doc->m_drawingInterl[staffSize];
		if (y_n > yh)
		{	yn = ynt + y_p;
			v_decal = - m_doc->m_drawingInterl[staffSize];
		}
		else
			yn = y_p - m_doc->m_drawingStaffSize[staffSize] - ynt;

		//hPen = (HPEN)SelectObject (hdc, CreatePen (PS_SOLID, _param.EpLignesPORTEE+1, workColor2));
		//xng = toZoom(xng);
		//xnd = toZoom(xnd);

        dc->SetPen( m_currentColour, ToDeviceContextX( m_doc->m_env.m_staffLineWidth ), AxSOLID );
        dc->SetBrush(m_currentColour , AxTRANSPARENT );

		for (i = 0; i < test; i++)
		{
			dc->DrawLine( ToDeviceContextX(xng) , ToDeviceContextY ( yn ) , ToDeviceContextX(xnd) , ToDeviceContextY ( yn ) );
			//h_line ( dc, xng, xnd, yn, _param.EpLignesPORTEE);
			//yh =  toZoom(yn);
			//MoveToEx (hdc, xng, yh, NULL);
			//LineTo (hdc, xnd, yh);

			yn += v_decal;
		}

        dc->ResetPen();
        dc->ResetBrush();
	}
	return;
}
    
void View::DrawMRest(DeviceContext *dc, LayerElement *element, Layer *layer, Staff *staff, Measure *measure)
{
    assert(layer); // Pointer to layer cannot be NULL"
    assert(staff); // Pointer to staff cannot be NULL"
    assert(dynamic_cast<MRest*>(element)); // Element must be a MultiRest"
    
    MRest *mrest = dynamic_cast<MRest*>(element);
    
    dc->StartGraphic( element, "mrest", element->GetUuid() );
    
    //LogMessage("Measure %d - X %d - RightX %d;", measure->GetIdx(), element->m_drawingX, measure->GetRightBarlineX() );
    
    int x = element->GetDrawingX() + mrest->m_hOffset;
    int xCentered = x + (measure->GetDrawingX() + measure->GetRightBarlineX() - x)  / 2;
    int y = element->GetDrawingY();
    
    // move it down according to the number of line in the staff
    y -= staff->m_drawingLines / 2 * m_doc->m_drawingInterl[staff->staffSize];
    
    DrawWholeRest ( dc, xCentered, y, DUR_1, 0, false, staff);
    
    dc->EndGraphic(element, this);
    
    return;
    
}

/** This function draws multi-measure rests
 **/
void View::DrawMultiRest(DeviceContext *dc, LayerElement *element, Layer *layer, Staff *staff, Measure *measure)
{	
    int x1, x2, y1, y2, length;

    assert(layer); // Pointer to layer cannot be NULL"
    assert(staff); // Pointer to staff cannot be NULL"
    assert(dynamic_cast<MultiRest*>(element)); // Element must be a Symbol"
    
    MultiRest *multirest = dynamic_cast<MultiRest*>(element);
    
    dc->StartGraphic( element, "multirest", element->GetUuid() );
    
    int x = element->GetDrawingX() + multirest->m_hOffset;
    
    // We do not support more than three chars
    if (multirest->GetNumber() > 999)
        multirest->SetNumber(999);
    
    // This is 1/2 the length of th black rectangle
	length = (m_doc->m_drawingStep1 * 5);
    
    // Position centered in third line
    // it would be m_drawingInterl * 6.5, or m_drawingInterl / 2 * 13
	y1 = staff->GetDrawingY() - (m_doc->m_drawingInterl[staff->staffSize] / 2) * 5;
    y2 = y1 + m_doc->m_drawingInterl[staff->staffSize];
	
    // a is the central point, claculate x and x2
    x1 = x - length;
    x2 = x + length;
    
    // Draw the base rect
    // make it 8 pixels smaller than the interline space
    // these are the two 4 substracted and added
	DrawFullRectangle(dc, x1, y2 - 4, x2, y1 + 4);
    
    //Draw the to lines at beginning and end
    // make it 8 pixesl longers, and 4 pixels width
    DrawVerticalLine(dc, y1 - 4, y2 + 4, x1, 4);
    DrawVerticalLine(dc, y1 - 4, y2 + 4, x2, 4);
    
    // Draw the text above
    int w, h;
    unsigned int start_offset = 0; // offset from x to center text
    
    // convert to string
    std::stringstream text;
    text << multirest->GetNumber();
    
    dc->GetTextExtent( text.str(), &w, &h);
    start_offset = (x2 - x1 - w) / 2; // calculate offset to center text
    
    DrawLeipzigString(dc, x1 + start_offset, staff->GetDrawingY() + 5, text.str(), false);
    
    dc->EndGraphic(element, this);
    
    return;

}

void View::DrawLongRest ( DeviceContext *dc, int x, int y, Staff *staff)
{
    int x1, x2, y1, y2;
    
    y1 = y;
	x1 = x;
	x2 = x + (m_doc->m_drawingStep1 * 2 / 3);
    
    // look if on line or between line
	if ( (y - staff->GetDrawingY()) % m_doc->m_drawingInterl[staff->staffSize])
		y1 -= m_doc->m_drawingHalfInterl[staff->staffSize];
    
	y2 = y1 + m_doc->m_drawingInterl[staff->staffSize]*2;
	DrawFullRectangle( dc, x1, y2, x2, y1);
	return;
}


void View::DrawBreveRest ( DeviceContext *dc, int x, int y, Staff *staff)
{	
    int x1, x2, y1, y2;
    y1 = y;
	x1 = x;
	x2 = x + (m_doc->m_drawingStep1 * 2 / 3);
    
    // look if one line or between line
	if ( (y - staff->GetDrawingY()) % m_doc->m_drawingInterl[staff->staffSize])
		y1 -= m_doc->m_drawingHalfInterl[staff->staffSize];
    
	y2 = y1 + m_doc->m_drawingInterl[staff->staffSize]*2;
	DrawFullRectangle( dc, x1, y2, x2, y1);
	
    // lines
    x1 = x - m_doc->m_drawingStep1;
    x2 = x + m_doc->m_drawingStep1;

	DrawHorizontalLine ( dc, x1, x2, y2, 1);
	DrawHorizontalLine ( dc, x1, x2, y1, 1);
	return;
}

void View::DrawWholeRest ( DeviceContext *dc, int x, int y, int valeur, unsigned char dots, unsigned int smaller, Staff *staff)
{
    int x1, x2, y1, y2, vertic;
    y1 = y;
    vertic = m_doc->m_drawingHalfInterl[staff->staffSize];
	
    int off;
	float foff;

	if (staff->notAnc)
		foff = (m_doc->m_drawingStep1 *1 / 3);
	else
		foff = (m_doc->m_drawingLedgerLine[staff->staffSize][2] * 2) / 3; // i.e., la moitie de la ronde

	if (smaller)
		foff *= (int)( (float)m_doc->m_drawingGraceRatio[0] / (float)m_doc->m_drawingGraceRatio[1] );
	off = (int)foff;

	x1 = x - off;
	x2 = x + off;

	if (valeur == DUR_1)
		vertic = -vertic;

    // look if one line or between line
	if ( (y - staff->GetDrawingY()) % m_doc->m_drawingInterl[staff->staffSize])
	{
		if (valeur == DUR_2)
			y1 -= vertic;
		else
			y1 += vertic;
	}

	y2 = y1 + vertic;
	DrawFullRectangle ( dc, x1, y1, x2, y2);

	off /= 2;
	x1 -= off;
	x2 += off;

    // legder line
	if (y > (int)staff->GetDrawingY()  || y < staff->GetDrawingY() - m_doc->m_drawingStaffSize[staff->staffSize])
		DrawHorizontalLine ( dc, x1, x2, y1, m_doc->m_env.m_staffLineWidth);

	if (dots)
		DrawDots ( dc, (x2 + m_doc->m_drawingStep1), y2, dots, staff);
}


void View::DrawQuarterRest ( DeviceContext *dc, int x, int y, int valeur, unsigned char dots, unsigned int smaller, Staff *staff)
{
	DrawLeipzigFont( dc, x, y, LEIPZIG_REST_QUARTER + (valeur-DUR_4), staff, smaller );

	if (dots)
	{	if (valeur < DUR_16)
			y += m_doc->m_drawingInterl[staff->staffSize];
		DrawDots ( dc, (x + m_doc->m_drawingStep2), y, dots, staff);
	}
	return;
}


void View::DrawDots ( DeviceContext *dc, int x, int y, unsigned char dots, Staff *staff )

{
    if ( (y - staff->GetDrawingY()) % m_doc->m_drawingInterl[staff->staffSize] == 0 ) {
        y += m_doc->m_drawingHalfInterl[staff->staffSize];
    }
    
	int i;
	for (i = 0; i < dots; i++) {
		DrawDot ( dc, x, y );
		x += std::max (6, 2 * m_doc->m_drawingStep1);
	}
	return;
}



void View::CalculateLigaturePosX ( LayerElement *element, Layer *layer, Staff *staff)
{
    /*
	if (element == NULL) 
    {
    	return;
    }
    LayerElement *previous = layer->GetPrevious(element);
	if (previous == NULL || !previous->IsNote()) 
    {
        return;
    }
    Note *previousNote = dynamic_cast<Note*>(previous);
    if (previousNote->m_lig==LIG_TERMINAL)
    {
        return;
    } 
	if (previousNote->m_lig && previousNote->m_dur <= DUR_1)
	{	
        element->SetDrawingX( previous->GetDrawingX() + m_doc->m_drawingBrevisWidth[staff->staffSize] * 2 );
	}
    */
    return;
}

void View::DrawLigature ( DeviceContext *dc, int y, LayerElement *element, Layer *layer, Staff *staff )
{	
    assert(layer); // Pointer to layer cannot be NULL"
    assert(staff); // Pointer to staff cannot be NULL"
    assert(dynamic_cast<Note*>(element)); // Element must be a Note"
    
    	/*
    Note *note = dynamic_cast<Note*>(element);
    

	int xn, x1, x2, yy2, y1, y2, y3, y4, y5;
	int milieu, up, epaisseur;

	epaisseur = std::max (2, m_doc->m_drawingBeamWidth[staff->staffSize]/2);
	xn = element->GetDrawingX();

	if ((note->m_lig==LIG_MEDIAL) || (note->m_lig==LIG_TERMINAL))
    {
		CalculateLigaturePosX ( element, layer, staff );
    }
	else {
		xn = element->GetDrawingX() + note->m_hOffset;
    }

	// calcul des dimensions du rectangle
	x1 = xn - m_doc->m_drawingBrevisWidth[staff->staffSize]; x2 = xn +  m_doc->m_drawingBrevisWidth[staff->staffSize];
	y1 = y + m_doc->m_drawingHalfInterl[staff->staffSize]; 
	y2 = y - m_doc->m_drawingHalfInterl[staff->staffSize]; 
	y3 = (int)(y1 + m_doc->m_drawingVerticalUnit1[staff->staffSize]);	// partie d'encadrement qui depasse
	y4 = (int)(y2 - m_doc->m_drawingVerticalUnit1[staff->staffSize]);	

	if (!note->m_ligObliqua && (!View::s_drawingLigObliqua))	// notes rectangulaires, y c. en ligature
	{
		if (note->GetColored()!=BOOLEAN_true)
		{				//	double base des carrees
			DrawObliqueLine ( dc, x1,  y1,  x2,  y1, -epaisseur );
			DrawObliqueLine ( dc, x1,  y2,  x2,  y2, epaisseur );
		}
		else
			DrawFullRectangle( dc,x1,y1,x2,y2);	// dessine val carree pleine // ENZ correction de x2

		DrawVerticalLine ( dc, y3, y4, x1, m_doc->m_env.m_stemWidth );	// corset lateral
		DrawVerticalLine ( dc, y3, y4, x2, m_doc->m_env.m_stemWidth );
	}
	else			// traitement des obliques
	{
		if (!View::s_drawingLigObliqua)	// 1e passage: ligne verticale initiale
		{
			DrawVerticalLine (dc,y3,y4,x1, m_doc->m_env.m_stemWidth );
			View::s_drawingLigObliqua = true;
			//oblique = OFF;
//			if (val == DUR_1)	// queue gauche haut si DUR_1
//				queue_lig = ON;
		}
		else	// 2e passage: lignes obl. et verticale finale
		{
			x1 -=  m_doc->m_drawingBrevisWidth[staff->staffSize]*2;	// avance auto

			y1 = *View::s_drawingLigY - m_doc->m_drawingHalfInterl[staff->staffSize];	// ligat_y contient y original
			yy2 = y2;
			y5 = y1+ m_doc->m_drawingInterl[staff->staffSize]; y2 += m_doc->m_drawingInterl[staff->staffSize];	// on monte d'un INTERL

			if (note->GetColored()==BOOLEAN_true)
				DrawObliqueLine ( dc,  x1,  y1,  x2,  yy2, m_doc->m_drawingInterl[staff->staffSize]);
			else
			{	DrawObliqueLine ( dc,  x1,  y1,  x2,  yy2, 5);
				DrawObliqueLine ( dc,  x1,  y5,  x2,  y2, -5);
			}
			DrawVerticalLine ( dc,y3,y4,x2,m_doc->m_env.m_stemWidth);	//cloture verticale

			View::s_drawingLigObliqua = false;
//			queue_lig = OFF;	//desamorce alg.queue DUR_BR

		}
	}

	if (note->m_lig)	// memoriser positions d'une note a l'autre; relier notes par barres
	{	
        *(View::s_drawingLigX+1) = x2; *(View::s_drawingLigY+1) = y;	// relie notes ligaturees par barres verticales
		//if (in(x1,(*View::s_drawingLigX)-2,(*View::s_drawingLigX)+2) || (this->fligat && this->lat && !Note1::marq_obl))
			// les dernieres conditions pour permettre ligature verticale ancienne
		//	DrawVerticalLine (dc, *ligat_y, y1, (this->fligat && this->lat) ? x2: x1, m_doc->m_parameters.m_stemWidth); // ax2 - drawing vertical lines missing
		*View::s_drawingLigX = *(View::s_drawingLigX + 1);
		*View::s_drawingLigY = *(View::s_drawingLigY + 1);
	}

	
	y3 = y2 - m_doc->m_drawingHalfInterl[staff->staffSize]*6;

	if (note->m_lig)
	{	
        if (note->m_dur == DUR_BR) //  && this->queue_lig)	// queue gauche bas: DUR_BR initiale descendante // ax2 - no support of queue_lig (see WG corrigeLigature)
		{
            DrawVerticalLine ( dc, y2, y3, x1, m_doc->m_env.m_stemWidth );
        }
		else if (note->m_dur == DUR_LG) // && !this->queue_lig) // DUR_LG en ligature, queue droite bas // ax2 - no support of queue_lig
		{
            DrawVerticalLine (dc, y2, y3, x2, m_doc->m_env.m_stemWidth );
        }
		else if (note->m_dur == DUR_1) // && this->queue_lig )	// queue gauche haut // ax2 - no support of queue_lig
		{	
            y2 = y1 + m_doc->m_drawingHalfInterl[staff->staffSize]*6;
			DrawVerticalLine ( dc, y1, y2, x1, m_doc->m_env.m_stemWidth );
		} 
	}
	else if (note->m_dur == DUR_LG)		// DUR_LG isolee: queue comme notes normales
	{	
		milieu = staff->GetDrawingY() - m_doc->m_drawingInterl[staff->staffSize]*6;
		/// up = this->q_auto ? ((y < milieu)? ON :OFF):this->queue;
		// ENZ
		up = (y < milieu) ? ON : OFF;
		// ENZ
		if ( !note->m_stemDir == 0 )
			up = (up == ON) ? OFF : ON;
			
		if (up)
		{	
            y3 = y1 + m_doc->m_drawingHalfInterl[staff->staffSize]*6;
			y2 = y1;
		}
		DrawVerticalLine ( dc, y2,y3,x2, m_doc->m_env.m_stemWidth );
	}
    */

	return;
}

void View::DrawBarline( DeviceContext *dc, LayerElement *element, Layer *layer, Staff *staff )
{
    assert(layer); // Pointer to layer cannot be NULL"
    assert(staff); // Pointer to staff cannot be NULL"
    assert(staff->m_parent); // Pointer to system cannot be NULL"
    assert(dynamic_cast<Barline*>(element)); // Element must be a Barline"
    
    Barline *barline = dynamic_cast<Barline*>(element);
    int x = element->GetDrawingX() + barline->m_hOffset;

    dc->StartGraphic( element, "barline", element->GetUuid() );
    
    if (barline->m_partialBarline)
    {
        DrawPartialBarline ( dc, dynamic_cast<System*>( staff->m_parent ), x, staff);
    }
    else
    {
        //DrawBarline( dc, (System*)staff->m_parent, x,  m_doc->m_env.m_barlineWidth, barline->m_onStaffOnly, staff);
    }
    

    
    dc->EndGraphic(element, this ); //RZ
}

void View::DrawClef( DeviceContext *dc, LayerElement *element, Layer *layer, Staff *staff )
{
    assert(layer); // Pointer to layer cannot be NULL"
    assert(staff); // Pointer to staff cannot be NULL"
    assert(dynamic_cast<Clef*>(element)); // Element must be a Clef"
    
    Clef *clef = dynamic_cast<Clef*>(element);

    dc->StartGraphic( element, "clef", element->GetUuid() );
	
	int b = staff->GetDrawingY();
	int a = element->GetDrawingX();
    int sym = LEIPZIG_CLEF_G;	//sSOL, position d'ordre des cles sol fa ut in fonts

    /*  poser sym=no de position sSOL dans la fonte
     *	au depart; ne faire operation sur b qu'une fois pour cas semblables,
     *  et au palier commun superieur, incrementer sym, sans break.
     */
	switch(clef->GetClefId())
	{
		case C1 :
            sym += 2;
		case G1 :
            b -= m_doc->m_drawingStaffSize[ staff->staffSize ]; 
            break;
		case G2va :
            sym += 1;
		case C2 :
            sym += 2;
		case G2 :
            b -= m_doc->m_drawingInterl[ staff->staffSize ]*3; 
            break;
		case F3 :
            sym--;
		case C3 :
            b -= m_doc->m_drawingInterl[ staff->staffSize ]*2; 
            sym += 2; 
            break;
		case F5 :
            sym++; 
            break;
		case F4 :
            sym--;
		case C4 :
            b -= m_doc->m_drawingInterl[ staff->staffSize ];
		case C5 :
            sym += 2; 
            break;
		case perc :
            b -= m_doc->m_drawingInterl[ staff->staffSize ]*2;
            sym = LEIPZIG_CLEF_PERC; 
            break;
		default: 
            break;
	}

	a -= m_doc->m_drawingStep1*2;
	if (clef->m_cueSize)
		a+= m_doc->m_drawingStep1;

	DrawLeipzigFont ( dc, a, b, sym, staff, clef->m_cueSize  );
   
    dc->EndGraphic(element, this ); //RZ
}

void View::DrawMensur( DeviceContext *dc, LayerElement *element, Layer *layer, Staff *staff )
{
    assert(layer); // Pointer to layer cannot be NULL"
    assert(staff); // Pointer to staff cannot be NULL"
    assert(dynamic_cast<Mensur*>(element)); // Element must be a Mensur"

    Mensur *mensur = dynamic_cast<Mensur*>(element);
 
    dc->StartGraphic( element, "mensur", element->GetUuid() );
	
	int x;

    if (mensur->GetSign()==MENSURATIONSIGN_O)
    {	
        DrawMensurCircle ( dc, element->GetDrawingX(), staff->GetDrawingY(), staff);
    }
    else if (mensur->GetSign()==MENSURATIONSIGN_C && !mensur->GetOrient()==ORIENTATION_reversed)
    {	
        DrawMensurHalfCircle ( dc, element->GetDrawingX(), staff->GetDrawingY(), staff);
    }
    else if (mensur->GetSign()==MENSURATIONSIGN_C && mensur->GetOrient()==ORIENTATION_reversed)
    {	
        DrawMensurReversedHalfCircle ( dc, element->GetDrawingX(), staff->GetDrawingY(), staff);
    }
    if (mensur->GetSlash()) // we handle only one single slash
    {	
        DrawMensurSlash ( dc, element->GetDrawingX(), staff->GetDrawingY(), staff);
    }
    if (mensur->GetDot()) // we handle only one single dot
    {	
        DrawMensurDot (dc, element->GetDrawingX(), staff->GetDrawingY(), staff);
    }

	if (mensur->GetNumInt())
	{	
        x = element->GetDrawingX();
		if (mensur->GetSign())
        {
			x += m_doc->m_drawingStep1*5; // step forward because we have a sign or a meter symbol
        }
		DrawMensurFigures ( dc, x, staff->GetDrawingY(), mensur->GetNumInt(), mensur->GetNumbaseInt(), staff);
	}
    
    dc->EndGraphic(element, this ); //RZ

}


void View::DrawMensurCircle( DeviceContext *dc, int x, int yy, Staff *staff )
{
	assert( dc ); // DC cannot be NULL
	
	int y =  ToDeviceContextY (yy - m_doc->m_drawingInterl[ staff->staffSize ] * 2);
	int r = ToDeviceContextX( m_doc->m_drawingInterl[ staff->staffSize ]);

	int w = std::max( ToDeviceContextX(4), 2 );

    dc->SetPen( m_currentColour, w, AxSOLID );
    dc->SetBrush( m_currentColour, AxTRANSPARENT );

	dc->DrawCircle( ToDeviceContextX(x), y, r );

    dc->ResetPen();
    dc->ResetBrush();
}	

void View::DrawMensurHalfCircle( DeviceContext *dc, int x, int yy, Staff *staff )
{
	assert( dc ); // DC cannot be NULL

	int w = std::max( ToDeviceContextX(4), 2 );
    dc->SetPen( m_currentColour, w, AxSOLID );
    dc->SetBrush( m_currentColour, AxTRANSPARENT );

	int y =  ToDeviceContextY (yy - m_doc->m_drawingInterl[ staff->staffSize ]);
	int r = ToDeviceContextX( m_doc->m_drawingInterl[ staff->staffSize ]);

	x = ToDeviceContextX (x);
	x -= 3*r/3;

	dc->DrawEllipticArc( x, y, 2*r, 2*r, 70, 290 );
		
    dc->ResetPen();
    dc->ResetBrush();

	return;
}	

void View::DrawMensurReversedHalfCircle( DeviceContext *dc, int x, int yy, Staff *staff )
{	
	assert( dc ); // DC cannot be NULL

	int w = std::max( ToDeviceContextX(4), 2 );
    dc->SetPen( m_currentColour, w, AxSOLID );
    dc->SetBrush( m_currentColour, AxTRANSPARENT );

	int y =  ToDeviceContextY (yy - m_doc->m_drawingInterl[ staff->staffSize ]);
	int r = ToDeviceContextX( m_doc->m_drawingInterl[ staff->staffSize ] );

    // needs to be fixed
	x = ToDeviceContextX (x);
	x -= 4*r/3;

	dc->DrawEllipticArc( x, y, 2*r, 2*r, 250, 110 );
    
    dc->ResetPen();
    dc->ResetBrush();

	return;
}	

void View::DrawMensurDot ( DeviceContext *dc, int x, int yy, Staff *staff )
{
	assert( dc ); // DC cannot be NULL

	int y =  ToDeviceContextY (yy - m_doc->m_drawingInterl[ staff->staffSize ] * 2);
	int r = std::max( ToDeviceContextX(4), 2 );
	
    dc->SetPen( m_currentColour, 1, AxSOLID );
    dc->SetBrush( m_currentColour, AxSOLID );

	dc->DrawCircle( ToDeviceContextX(x) -r/2 , y, r );
		
    dc->ResetPen();
    dc->ResetBrush();

	return;
}	


void View::DrawMensurSlash ( DeviceContext *dc, int a, int yy, Staff *staff )
{	
	assert( dc ); // DC cannot be NULL
	
	int y1 = yy;
	int y2 = y1 - m_doc->m_drawingStaffSize[ staff->staffSize ];
	
	DrawVerticalLine ( dc, y1, y2, a, 3);
	return;
}	


void View::DrawMensurFigures( DeviceContext *dc, int x, int y, int num, int numBase, Staff *staff)
{
	assert( dc ); // DC cannot be NULL	
    	
	int ynum, yden;
	std::string s;

	if (numBase)
	{	
		ynum = y - (m_doc->m_drawingHalfInterl[staff->staffSize]*4);
		yden = ynum - (m_doc->m_drawingInterl[staff->staffSize]*2);
	}
	else
		ynum = y - (m_doc->m_drawingHalfInterl[staff->staffSize]*6);

	if (numBase > 9 || num > 9)	// avancer
		x += m_doc->m_drawingStep1*2;

	s = StringFormat("%u",num);
	DrawLeipzigString ( dc, x, ynum, s, 1, staff->staffSize);	// '1' = centrer

	if (numBase)
	{
        s = StringFormat("%u",numBase);
		DrawLeipzigString ( dc, x, yden, s, 1, staff->staffSize);	// '1' = centrer
	}
	return;
}
    
    
void View::DrawMeterSig( DeviceContext *dc, LayerElement *element, Layer *layer, Staff *staff )
{
    assert(layer); // Pointer to layer cannot be NULL"
    assert(staff); // Pointer to staff cannot be NULL"
    assert(dynamic_cast<MeterSig*>(element)); // Element must be a Mensur"
    
    MeterSig *meterSig = dynamic_cast<MeterSig*>(element);
    
    dc->StartGraphic( element, "meterSig", element->GetUuid() );
    
    int y = staff->GetDrawingY() - (m_doc->m_drawingHalfInterl[ staff->staffSize ]*6);
    int x = element->GetDrawingX();
    
    if ( meterSig->GetSym() == METERSIGN_common ) {
        DrawLeipzigFont( dc, element->GetDrawingX(), y, LEIPZIG_METER_SYMB_COMMON, staff, staff->staffSize);
        x += m_doc->m_drawingStep1*5; // step forward because we have a symbol
    }
    else if ( meterSig->GetSym() == METERSIGN_cut ) {
        DrawLeipzigFont( dc, element->GetDrawingX(), y, LEIPZIG_METER_SYMB_CUT, staff, staff->staffSize);
        x += m_doc->m_drawingStep1*5; // step forward because we have a symbol
    }

    if (meterSig->GetCount())
    {	
        DrawMensurFigures ( dc, x, staff->GetDrawingY(), meterSig->GetCount(), meterSig->GetUnit(), staff);
    }
    
    dc->EndGraphic(element, this );
    
}


void View::DrawSymbol( DeviceContext *dc, LayerElement *element, Layer *layer, Staff *staff, LayerElement *parent )
{
    assert(layer); // Pointer to layer cannot be NULL"
    assert(staff); // Pointer to staff cannot be NULL"
    assert(dynamic_cast<Symbol*>(element)); // Element must be a Symbol"
    
    // This is used when we add dynamically an element (eg. accidentals before notes)
    // So we can get the clef without adding the new elem in the list
    LayerElement *list_elem = element;
    if (parent) list_elem = parent;
    
    Symbol *symbol = dynamic_cast<Symbol*>(element);
    int oct = symbol->m_oct - 4;
    element->SetDrawingY( element->GetDrawingY() + CalculatePitchPosY( staff, symbol->m_pname, layer->GetClefOffset( list_elem ), oct) );
    
    if (symbol->m_type==SYMBOL_ACCID) {
        DrawSymbolAccid(dc, element, layer, staff);
    }
    else if (symbol->m_type==SYMBOL_CUSTOS) {
        DrawSymbolCustos(dc, element, layer, staff);
    }
    else if (symbol->m_type==SYMBOL_DOT) {
        DrawSymbolDot(dc, element, layer, staff);
    }
    
}


void View::DrawSymbolAccid( DeviceContext *dc, LayerElement *element, Layer *layer, Staff *staff )
{
    assert(layer); // Pointer to layer cannot be NULL"
    assert(staff); // Pointer to staff cannot be NULL"
    assert(dynamic_cast<Symbol*>(element)); // Element must be a Symbol"
    
    Symbol *accid = dynamic_cast<Symbol*>(element);
    dc->StartGraphic( element, "accid", element->GetUuid() );
    
    int x = element->GetDrawingX() + accid->m_hOffset;
    int y = element->GetDrawingY();
    
    int symc;
    switch (accid->m_accid)
    {	case ACCID_NATURAL :  symc = LEIPZIG_ACCID_NATURAL; break;
        
        /* The ACCID_DOUBLE_SHARP definition is used in two ways:
         * 1) Antique notation (notAnc == true): it displays the two
         *    stacked sharps used in key signatures;
         * 2) Modern notation: is display a double flat
         *    this poses no particular rendering problems;
         * Case 1) on the other hand requires that the sharp
         * is printed two times, the second time shifting it up
         * (hence no break in the case statement)
         * Same thing applies to LEIPZIG_ACCID_FLAT, but in this
         * case it is used ONLY ad stacked flats (no double flat
         * glyph).
         */
        case ACCID_DOUBLE_SHARP :
            if (staff->notAnc) {
                    symc = LEIPZIG_ACCID_SHARP;
                    DrawLeipzigFont ( dc, x, y, symc, staff, accid->m_cueSize );    
                    y += 7*m_doc->m_drawingHalfInterl[staff->staffSize]; // LP
            } else {
                symc = LEIPZIG_ACCID_DOUBLE_SHARP;
                break;
            }
        case ACCID_SHARP : symc = LEIPZIG_ACCID_SHARP; break;
        case ACCID_DOUBLE_FLAT :  
            symc = LEIPZIG_ACCID_FLAT; DrawLeipzigFont ( dc, x, y, symc, staff, accid->m_cueSize );
            y += 7*m_doc->m_drawingHalfInterl[staff->staffSize]; // LP
        case ACCID_FLAT :  symc = LEIPZIG_ACCID_FLAT; break;
        case ACCID_QUARTER_SHARP : symc = LEIPZIG_ACCID_QUARTER_SHARP; break;
        case ACCID_QUARTER_FLAT :  symc= LEIPZIG_ACCID_QUARTER_FLAT; break;
    }
    DrawLeipzigFont ( dc, x, y, symc, staff, accid->m_cueSize );

    
    dc->EndGraphic(element, this );

}


void View::DrawSymbolCustos( DeviceContext *dc, LayerElement *element, Layer *layer, Staff *staff )
{
    assert(layer); // Pointer to layer cannot be NULL"
    assert(staff); // Pointer to staff cannot be NULL"
    assert(dynamic_cast<Symbol*>(element)); // Element must be a Symbol"
    
    Symbol *custos = dynamic_cast<Symbol*>(element);
    dc->StartGraphic( element, "custos", element->GetUuid() );

    int x = element->GetDrawingX() + custos->m_hOffset;
    int y = element->GetDrawingY();
    y -= m_doc->m_drawingHalfInterl[staff->staffSize] - m_doc->m_drawingVerticalUnit2[staff->staffSize];  // LP - correction in 2.0.0
    
    DrawLeipzigFont( dc, x, y, 35, staff, custos->m_cueSize );
    
    dc->EndGraphic(element, this ); //RZ

}

void View::DrawSymbolDot( DeviceContext *dc, LayerElement *element, Layer *layer, Staff *staff )
{
    assert(layer); // Pointer to layer cannot be NULL"
    assert(staff); // Pointer to staff cannot be NULL"
    assert(dynamic_cast<Symbol*>(element)); // Element must be a Symbol"
    
    Symbol *dot = dynamic_cast<Symbol*>(element);
    dc->StartGraphic( element, "dot", element->GetUuid() );
    
    int x = element->GetDrawingX() + dot->m_hOffset;
    int y = element->GetDrawingY();

    switch (dot->m_dot)
    {	
        case 1 : DrawDot( dc, x, y ); x += std::max (6, m_doc->m_drawingStep1);
        case 0 : DrawDot ( dc, x, y );
    }
    
    dc->EndGraphic(element, this );

}
    
void View::DrawSyl( DeviceContext *dc, Syl *syl, LayerElement *element, Layer *layer, Staff *staff )
{
    
    int x = element->GetDrawingX();
    int y = element->GetDrawingY();
    if (staff->GetAlignment() ) {
        y = staff->GetDrawingY() - staff->GetAlignment()->GetMaxHeight();
    }
    dc->StartGraphic( syl, "syl", syl->GetUuid() );
    DrawLyricString(dc, x, y, UTF16to8( syl->GetText().c_str() ) );
    dc->EndGraphic(syl, this );

}
    
void View::DrawVerse( DeviceContext *dc, Verse *verse, LayerElement *element, Layer *layer, Staff *staff )
{
    if (verse->GetChildCount( &typeid(Syl) ) ) {
        DrawSyl(dc, dynamic_cast<Syl*>( verse->GetFirst( &typeid(Syl) ) ), element, layer, staff );
    }
}

    
void View::DrawKeySig( DeviceContext *dc, LayerElement *element, Layer *layer, Staff *staff )
{
    assert(layer); // Pointer to layer cannot be NULL"
    assert(staff); // Pointer to staff cannot be NULL"

    KeySig *ks = dynamic_cast<KeySig*>(element);
    int symb;
    int x, y;
    
    Clef *c = layer->GetClef(element);
    if (!c) {
        return;
    }
    
    dc->StartGraphic( element, "keysig", element->GetUuid() );
    
    for (int i = 0; i < ks->GetAlterationNumber(); i++) {
        
        x = element->GetDrawingX() + (m_doc->m_drawingAccidWidth[staff->staffSize][0] + 5) * i;
        y = staff->GetDrawingY() + CalculatePitchPosY( staff, ks->GetAlterationAt(i), layer->GetClefOffset( element ), ks->GetOctave(ks->GetAlterationAt(i), c->GetClefId()));;
        
        if (ks->GetAlteration() == ACCID_FLAT)
            symb = LEIPZIG_ACCID_FLAT;
        else
            symb = LEIPZIG_ACCID_SHARP;
        
        DrawLeipzigFont ( dc, x, y, symb, staff, false );
    }
    
    dc->EndGraphic(element, this ); //RZ
    
}

void View::DrawTie( DeviceContext *dc, LayerElement *element, Layer *layer, Staff *staff, Measure *measure )
{
    assert(layer); // Pointer to layer cannot be NULL"
    assert(staff); // Pointer to staff cannot be NULL"
    assert(dynamic_cast<Tie*>(element) || dynamic_cast<Slur*>(element)); // The element must be a tie
    
    bool up = false;
    LayerElement *note1 = NULL;
    LayerElement *note2 = NULL;
    
    // coordinates for the tie/slur
    int x1, x2, y1, y2;
    up = true;
    data_STEMDIRECTION noteStemDir = STEMDIRECTION_NONE;
    
    Tie *tie = dynamic_cast<Tie*>(element);
    Slur *slur = dynamic_cast<Slur*>(element);
    if ( tie ) {
        note1 = tie->GetFirstNote();
        note2 = tie->GetSecondNote();
    } else if ( slur ) {
        note1 = slur->GetFirstNote();
        note2 = slur->GetSecondNote();
    }
    
    if ( !note1 && !note2 ) {
        // no note, obviously nothing to do...
        return;
    }

    System *currentSystem = dynamic_cast<System*>( staff->GetFirstParent( &typeid(System) ) );
    System *parentSystem1 = NULL;
    System *parentSystem2 = NULL;
    
    // In order to know if we are drawing a normal tie or a tie splitted over two systems, we need
    // to look at the parent system of each note.
    if ( note1 ) {
        parentSystem1 = dynamic_cast<System*>( note1->GetFirstParent( &typeid(System) ) );
    }
    if ( note2 ) {
        parentSystem2 = dynamic_cast<System*>( note2->GetFirstParent( &typeid(System) ) );
    }

    // This is the case when the tie is split over two system of two pages.
    // In this case, we are now drawing its beginning to the end of the measure (i.e., the last aligner)
    if ( parentSystem2 && currentSystem && ( parentSystem2 != currentSystem) ) {
        Alignment *nextAlignement = note1->GetAlignment();
        while (nextAlignement) {
            if (nextAlignement->GetType() == ALIGNMENT_MEASURE_END) break;
            nextAlignement = dynamic_cast<Alignment*>(nextAlignement->GetNextSibling());
        }
        if (!nextAlignement) {
            return;
        }
        y1 = y2 = note1->GetDrawingY();
        x1 = note1->GetDrawingX();
        x2 = measure->GetDrawingX() + nextAlignement->GetXRel();
        assert(dynamic_cast<Note*>(note1));
        noteStemDir = dynamic_cast<Note*>(note1)->d_stemDir;
        //DrawTieOrSlurBezier(dc, note1->GetDrawingX(), y - 14, x2, y - 14, true);
    }
    // Now this is the case when the tie is split but we are drawing the end of it
    else if ( parentSystem1 && currentSystem && ( parentSystem1 != currentSystem) ) {
        Alignment *previousAlignement = note2->GetAlignment();
        while (previousAlignement) {
            if (previousAlignement->GetType() != ALIGNMENT_DEFAULT) break;
            previousAlignement = dynamic_cast<Alignment*>(previousAlignement->GetPreviousSibling());
        }
        if (!previousAlignement) {
            return;
        }
        y1 = y2 = note2->GetDrawingY();
        x2 = note2->GetDrawingX();
        x1 = x2 - m_doc->m_drawingStep2;
        assert(dynamic_cast<Note*>(note2));
        noteStemDir = dynamic_cast<Note*>(note2)->d_stemDir;
        //DrawTieOrSlurBezier(dc, x1, y - 14, note2->GetDrawingX(), y - 14, true);
    }
    // Finally the normal case
    else {
        assert( note1 && note2 );
        // Copied from DrawNote
        // We could use the stamDir information
        // but then we have to take in account (1) beams (2) stemmed and non stemmed notes tied together
        y1 = note1->GetDrawingY();
        y2 = note2->GetDrawingY();
        x1 = note1->GetDrawingX();
        x2 = note2->GetDrawingX();
        assert(dynamic_cast<Note*>(note1));
        // for now we only look at the first note - needs to be improved
        // d_stemDir it not set properly in beam - needs to be fixed.
        noteStemDir = dynamic_cast<Note*>(note1)->d_stemDir;
        /*
        assert(dynamic_cast<Note*>(note2));
        if (dynamic_cast<Note*>(note2)->d_stemDir != noteStemDir) {
            LogDebug("Diverging stem directions (%d;%d)", noteStemDir, dynamic_cast<Note*>(note2)->d_stemDir);
        }
        */
    }
    
    assert( dynamic_cast<Note*>(note1));
    if (noteStemDir == STEMDIRECTION_up) {
        up = false;
    }
    else if (noteStemDir == STEMDIRECTION_NONE) {
        // no information from the note stem directions, look at the position in the notes
        int center = staff->GetDrawingY() - m_doc->m_drawingInterl[staff->staffSize] * 2;
        up = (y1 > center) ? true : false;
    }
    
    // FIXME, take in account elements that can be netween notes, eg keys time etc
    // 20 height nice with 70, not nice with 50
    // Also remove hard-coded values!
    if (up) {
        y1 += 14;
        y2 += 14;
    }
    else {
        y1 -= 14;
        y2 -= 14;
    }
    
    dc->StartGraphic( element, "tie", element->GetUuid() );
    DrawTieOrSlurBezier(dc, x1, y2, x2, y2, !up);
    dc->EndGraphic(element, this );

    
}

void View::DrawAcciaccaturaSlash(DeviceContext *dc, LayerElement *element) {
    
    Note *note = dynamic_cast<Note*>(element);
    
    if (note->m_dur < DUR_8)
        return;
    
    dc->SetPen(AxBLACK, 2, AxSOLID);
    dc->SetBrush( AxBLACK, AxSOLID );
    
    if (element->m_drawingStemDir)
        dc->DrawLine(element->m_drawingStemStart.x - 10, ToDeviceContextY(element->m_drawingStemStart.y + 10), element->m_drawingStemStart.x + 20, ToDeviceContextY(element->m_drawingStemStart.y + 40));
    else
        dc->DrawLine(element->m_drawingStemStart.x - 10, ToDeviceContextY(element->m_drawingStemStart.y - 10), element->m_drawingStemStart.x + 20, ToDeviceContextY(element->m_drawingStemStart.y - 40));
    
    dc->ResetPen();
    dc->ResetBrush();
}

/** Draws a fermata
 rest - the fermata is always above the stavv
 note - for breves and semibreves, only above the staff
      - for flagged notes, the fermata is on the side of the notehead
 */
void View::DrawFermata(DeviceContext *dc, LayerElement *element, Staff *staff) {
    int x, y;
    int emb_offset = 0; // if there is and embellishment, offset the note up
    
    // We position the fermata in the same horizontal pos. of th eobject
    x = element->GetDrawingX();
    
    // First case, notes
    if (dynamic_cast<Note*>(element)) {
        Note *note = dynamic_cast<Note*>(element);
        
        /*
        // stem down or semibreve/longa, fermata up!
        if (!element->m_drawingStemDir && (note->m_dur != DUR_1 || note->m_dur != DUR_BR)) {
        */
            // only for up-fermatas, if there is a trill on the same note
            // add a 35 pixel space so they do not collide
            if (note->m_embellishment)
                emb_offset = 35;
            
            // check that the notehead is in the staff.
            if ((element->GetDrawingY()) < staff->GetDrawingY())
                // in the staff, set the fermata 20 pixels above the last line (+ embellishment offset)
                y = staff->GetDrawingY() + 20 + emb_offset;
            else
                // out of the staff, place the trill 20 px above the notehead
                y = (element->GetDrawingY()) + 20 + emb_offset;
            
            // draw the up-fermata
            DrawLeipzigFont ( dc, element->GetDrawingX(), y, LEIPZIG_FERMATA_UP, staff, false );
        /*
        } else { // stem up fermata down
            
            // This works as above, only we check that the note head is not
            // UNDER the staff
            if ((element->GetDrawingY()) > (staff->GetDrawingY() - m_doc->m_drawingStaffSize[staff->staffSize]))
                // notehead in staff, set at 20 px under
                y = staff->GetDrawingY() - m_doc->m_drawingStaffSize[staff->staffSize] - 20;
            else
                // notehead under staff, set 20 px under notehead
                y = (element->GetDrawingY()) - 20;
            
            DrawLeipzigFont ( dc, element->GetDrawingX(), y, LEIPZIG_FERMATA_DOWN, staff, false );
        }
        */
    } else if (dynamic_cast<Rest*>(element)) {
        // this is a rest
        // rests for the moment are always in the staff
        // so just place the fermata above the staff + 20 px
        y = staff->GetDrawingY() + 20;
        DrawLeipzigFont ( dc, element->GetDrawingX(), y, LEIPZIG_FERMATA_UP, staff, false );
    }
}

// Draw a trill above the notehead
// This function works as the up-fermata portion of DrawFermata
// if there are many symbols to draw we could make a generalized function
void View::DrawTrill(DeviceContext *dc, LayerElement *element, Staff *staff) {
    int x, y;    
    x = element->GetDrawingX();

    if ((element->GetDrawingY()) < staff->GetDrawingY())
        y = staff->GetDrawingY() + 30;
    else
        y = (element->GetDrawingY()) + 30;
    
    DrawLeipzigFont ( dc, element->GetDrawingX(), y, LEIPZIG_EMB_TRILL, staff, false );
}


void View::DrawLayerApp( DeviceContext *dc, LayerElement *element, Layer *layer, Measure *measure, Staff *staff ){
    
    assert(layer); // Pointer to layer cannot be NULL"
    assert(staff); // Pointer to staff cannot be NULL"
    assert(staff->m_parent); // Pointer to staff system cannot be NULL"
    
    LayerApp *app = dynamic_cast<LayerApp*>(element);    
    int i;
    for (i = 0; i < app->GetRdgCount(); i++ )
    {
        Layer *rdg = dynamic_cast<Layer*>( app->m_children[i] );
        assert( rdg );
        int j;
        for (j = 0; j < rdg->GetElementCount(); j++ ) {
            
            LayerElement *lelem = dynamic_cast<LayerElement*>( rdg->m_children[j] );
            assert( lelem );
            if (i == 0) {
                m_currentColour = AxGREEN;
            }
            else {
                m_currentColour = AxBLUE;
            }
            DrawElement(dc, lelem, layer, measure, staff );
            /*
            LayerElement rdgElement(&rdg->m_elements[j] );
            rdgElement.m_layer = element->m_layer;
            rdgElement.SetLayout( m_doc );
            rdgElement.m_drawingX = element->m_drawingX;
            DrawElement(dc, &rdgElement, layer, staff );
            rdgElement.m_layer = NULL;
            */
        }
        
        /*
        Staff *appStaff = new Staff( staff->m_n );
        appStaff->m_drawingY = staff->m_drawingY + m_doc->m_drawingStaffSize[staff->staffSize];
        appStaff->m_system = staff->m_system;
        appStaff->SetLayout( m_doc );
        DrawStaff(dc, appStaff, staff->m_system );
        delete appStaff;
        */
    }
}

} // namespace vrv    
    
/*
 * Lyric code not refactored in ax2
    
void Note1::DeleteLyricFromNote( Symbol1 *lyric )
{
	for ( int i = 0; i < (int)this->m_lyrics.GetCount(); i++ ){
		Symbol1 *tmp = &this->m_lyrics[i];
		if ( lyric == tmp ){
			this->m_lyrics.RemoveAt( i );
			break;
		}
	}
}

Symbol1 *Note1::GetFirstLyric( )
{
	if ( this->m_lyrics.GetCount() == 0 ) 
		return NULL;
	else
		return &this->m_lyrics[0];
}

Symbol1 *Note1::GetLastLyric( )
{
	int num = this->m_lyrics.GetCount(); 
	if ( num == 0 ) 
		return NULL;
	else 
		return &this->m_lyrics[num-1];
}

Symbol1 *Note1::GetLyricNo( int no )
{
	int num = this->m_lyrics.GetCount(); 
	if ( (no < 0) || (num <= no) ) 
		return NULL;
	else 
		return &this->m_lyrics[no];
}

*/
