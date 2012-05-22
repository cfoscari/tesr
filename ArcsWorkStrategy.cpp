//----------------------------------------------------------------------
//
// FILE:		ArcsWorkStrategy.cpp
// PROJECT:		CamPunchX
// OWNER:		Salvagnini Italia
// AUTHOR:		Andrea Giaretta
// DATE:		03/2008
//
//----------------------------------------------------------------------
//----------------------------------------------------------------------
// INCLUDES:
//----------------------------------------------------------------------
//Questo � un test
// 123
#include "stdafx.h"
// vediamo se funziona
#include <infline.h>
#include <utils.h>

#include <ind_conv.h>

#include <uISM.h>

#include "CamPunchAndCombi_def.h"

#ifdef COMPILING_CAMPUNCH_DLL
#include "CamPunch.h"
#include "PunchBorder.h"
#include "UtilsElaboration.h"
#endif
#ifdef COMPILING_CAM_COMBI_DLL
#include <resource.h>
#include "CamCombi.h"
#include "CombiBorder.h"
#include "CombiPart.h"
#include "CombiWaitThread.h"
#endif

#include "UtilsGeom.h"
#include "UtilsTools.h"
#include "UtilsGeneral.h"
#include "VarRect.h"


#ifdef COMPILING_CAMPUNCH_DLL
#define	_CAMPUNCH_TYPE_IMPLEMENTATION
#endif
#ifdef COMPILING_CAM_COMBI_DLL
#define	_CAMCOMBI_TYPE_IMPLEMENTATION
#endif

#include "ArcsWorkStrategy.h"

#ifdef COMPILING_CAMPUNCH_DLL
#undef _CAMPUNCH_TYPE_IMPLEMENTATION
#endif
#ifdef COMPILING_CAM_COMBI_DLL
#undef _CAMCOMBI_TYPE_IMPLEMENTATION
#endif

//----------------------------------------------------------------------
// CONSTANTS:
//----------------------------------------------------------------------
// Famiglie (locali) di utensile
enum {
	NONE		= 0,
	CIRCULAR	= 1,
	RECTANGULAR	= 2,
	MULT_RAD4	= 3,
	MULT_RAD8	= 4,
	TRICUSP		= 5
};

//----------------------------------------------------------------------
CArcsBorderWorkStrategy::CArcsBorderWorkStrategy()
{
	m_iToolType = CgTool::TYPE_PUNCH;
	m_bUseNibbling = false;
	m_bUseObroundNibbling = false;
	m_pSavedBorder = NULL;
}

//----------------------------------------------------------------------
int		
CArcsBorderWorkStrategy::DoAlgorithm()
{
	return SubstituteArcs(this->m_pBorder, m_iToolType, m_bUseNibbling, m_bUseObroundNibbling, m_pSavedBorder);	
}

//----------------------------------------------------------------------
void			
CArcsBorderWorkStrategy::SetToolType(int i)
{
	m_iToolType = i;
}

//----------------------------------------------------------------------
int				
CArcsBorderWorkStrategy::GetToolType() const
{
	return m_iToolType;
}

//----------------------------------------------------------------------
void			
CArcsBorderWorkStrategy::SetUseNibbling(bool b)
{
	m_bUseNibbling = b;
}

//----------------------------------------------------------------------
bool			
CArcsBorderWorkStrategy::GetUseNibbling() const
{
	return m_bUseNibbling;
}

//----------------------------------------------------------------------
void			
CArcsBorderWorkStrategy::SetUseObroundNibbling(bool b)
{
	m_bUseObroundNibbling = b;
}

//----------------------------------------------------------------------
bool			
CArcsBorderWorkStrategy::GetUseObroundNibbling() const
{
	return m_bUseObroundNibbling;
}

//----------------------------------------------------------------------
void			
CArcsBorderWorkStrategy::SetSavedBorder(CgBorder* pBorder)
{
	m_pSavedBorder = pBorder;
}

//----------------------------------------------------------------------
CgBorder*		
CArcsBorderWorkStrategy::GetSavedBorder() const
{
	return m_pSavedBorder;
}

//----------------------------------------------------------------------
//
// Description: Punzonatura di un arco
//
// Arguments:
//   border - bordo principale (modificato con la spezzata di sostituzione)
//   borderOrig - bordo originale con gli archi
//	 borderSubs - bordo sostituzione utilizzato
//   posArc - posizione dell'arco nel bordo originale
//   posLastSub - posizione, nel bordo modificato, dell'ultimo item 
//				  della spezzata di sostituzione
//   dMinDist - distanza minima di roditura
//	 iToolType - tipo di utensile
//   bUseNibbling - uso utensile in roditura
//
// Return value:
//	 1 se arco risolto completamente, 0 altrimenti, -1 aborted
//   lWIArc - lista di istruzioni che vengono programmate per risolvere l'arco
//
// Remarks:	Punzonatura di un arco
//
//----------------------------------------------------------------------
int 
CArcsBorderWorkStrategy::PunchArc(CAMBORDER &border, CgBorder &borderOrig, CgBorder &borderSubs, POSITION posArc, 
								  POSITION posLastSub, double dMinDist, int iToolType, bool bUseNibbling, 
								  PUNCH_INSTRUCTION_LIST &lWIArc)
{
	borderOrig.SetCurrItemPos(posArc);
	CgArc* pArc = (CgArc *) borderOrig.GetCurrItem();

	
	if (pArc->GetSignedAngle() >= 0)
		// Arco orientato in senso antiorario
		return PunchAnticlockwiseArc(border, borderOrig, borderSubs, posArc, posLastSub, dMinDist, iToolType, bUseNibbling, lWIArc);
	else
		// Arco orientato in senso orario
		return PunchClockwiseArc(border, borderOrig, borderSubs, posArc, posLastSub, dMinDist, iToolType, bUseNibbling, lWIArc);
}

//----------------------------------------------------------------------
//
// Description: Per ogni bordo del pannello gestisce gli item circolari (archi
//              e cerchi) e li sostituisce con spezzate
// Arguments:
//   iBorder - indice del bordo da processare
//   iToolType - tipo di utensile da utilizzare
//   bUseNibbling - flag che indica se l'utensile va usato anche in roditura
//   pSavedBorder - puntatore al bordo originale, prima di eventuali modifiche.
//
// Return: 0 = ok, -1 = aborted
//
// Remarks:	Prima della sostituzione di singoli archi vengono anche individuati
//          e gestiti profili ad asola e di tipo "a banana"
//
//----------------------------------------------------------------------
int 
CArcsBorderWorkStrategy::SubstituteArcs(CAMBORDER *pBorder, int iToolType, bool bUseNibbling, bool bUseObroundNibbling, CgBorder *pSavedBorder)
{
	// Se il bordo e` un cerchio, programmo direttamente la punzonatura
	if (pBorder->IsCircle()) 
	{
		PUNCH_INSTRUCTION_LIST lWIArc;
		bool bResolved = PunchCircle(*pBorder, iToolType, bUseNibbling, GetParameters().bSaveScrap, lWIArc);
		// In ogni caso aggancio le istruzioni individuate
		for(POSITION pos = lWIArc.GetHeadPosition(); pos != NULL; )
		{
			PUNCH_INSTRUCTION* pWI = lWIArc.GetNext(pos);
			if (pWI != NULL)
				GETPUNCHPROGRAM.SAVE_PUNCH_OPERATION(pWI, pBorder);
		}

		if (bResolved)
			pBorder->SetResolved();
		return 0;
	}

	if (WAIT_THREAD->IsStopped())
		return -1;

	// Se il bordo non contiene archi, passo immediatamente al bordo successivo
	if (pBorder->CountArcs() == 0)
		return 0;

	bool bOnlyArc = false;
	CgBorder *pBorderCopy = NULL;
	if (pBorder->CountArcs() == pBorder->CountItems()) {
		bOnlyArc = true;
	}

	// Se il bordo e` una scantonatura, faccio lo switch sul contorno piu` piccolo
	// per la gestione di asole (o meglio semiasole, perche` non e` possibile che una
	// scantonatura abbia forma ad asola). Sara` nell'eventuale gestione dei singoli
	// archi che si considereranno sia il contorno piccolo che quello grande.
	if (!pBorder->IsOriginal()) {
		pBorder->SetContour(1);
		pSavedBorder->SetContour(1);
	}

	// Identificazione e sostituzione di profili a banana
	int iRetVal = BananaMng(*pBorder, iToolType, bUseNibbling);
	if (iRetVal < 0)
		return -1;

	if (iRetVal == 1) {
		pBorder->SetResolved();
		return 0;
	}

	// Identificazione e sostituzione di profili ad asola
	iRetVal = RadiusRectMng(pBorder, iToolType, true, pSavedBorder);
	if (iRetVal < 0) 
		return -1;

	if (iRetVal == 1 || pBorder->IsResolved()) {
		pBorder->SetResolved();
		return 0;
	}

	// Identificazione e sostituzione di profili ad asola
	iRetVal = ObroundMng(pBorder, iToolType, bUseObroundNibbling, pSavedBorder);
	if (iRetVal < 0)
		return -1;

	if (iRetVal == 1 || pBorder->IsResolved()) {
		pBorder->SetResolved();
		return 0;
	}

	// Il bordo potrebbe pero' essere diventato un semplice cerchio per il gioco delle sostituzioni
	// eseguito dalle routine precedenti. Poiche' SingleArcMng non risolve cerchi completi, 
	// rifaccio check su cerchi.
	if (pBorder->IsCircle()) 
	{
		PUNCH_INSTRUCTION_LIST lWIArc;
		bool bResolved = PunchCircle(*pBorder, iToolType, bUseNibbling, GetParameters().bSaveScrap, lWIArc);

		// In ogni caso aggancio le istruzioni individuate (commenti gia' agganciati!).
		for(POSITION pos = lWIArc.GetHeadPosition(); pos != NULL; )
		{
			PUNCH_INSTRUCTION* pWI = lWIArc.GetNext(pos);
			if (pWI != NULL)
				GETPUNCHPROGRAM.SAVE_PUNCH_OPERATION(pWI, pBorder);
		}
		if (bResolved)
			pBorder->SetResolved();
		return 0;
	}

	// Identificazione e sostituzione di archi concentrici eseguibili con utensili a banana (sia proprie,
	// cioe' chiusi da archi, che improprie, cioe' chiuse da segmenti)
	iRetVal = PartialBananaMng(*pBorder, iToolType, bUseNibbling);
	if (iRetVal < 0)
		return -1;

	// Identificazione e sostituzione di archi singoli, a meno che il bordo non sia gia`
	// stato risolto dalla gestione asole e banane
	if (pBorder->IsResolved() == false && iToolType == CgTool::TYPE_PUNCH)
		iRetVal = SingleArcMng(*pBorder, iToolType, bUseNibbling);
	if (iRetVal < 0)
		return -1;

	// Verifico se il bordo e' stato tutto risolto (qualora esso all'origine era formato 
	// da soli archi).
	if (pBorder->IsResolved() == false && bOnlyArc)
	{
		// Richiedo le face corrispondenti alle punzonature gia' memorizzate.
		// Modifica necessaria solo per nuova versione CAM3D::Offset
		pBorderCopy = pBorder->Offset(-0.5*resabs);
		if (pBorderCopy != NULL) 
		{
			// poligono delle punzonature
			CbPolygon polyPunches;
			CUtilsGeom::GetResolvedArea(pBorder, GetPart(), iToolType, polyPunches);

			if (polyPunches.IsEmpty() == false)
			{	
				// poligono del bordo
				CbPolygon *pPolyBorder = pBorderCopy->GetPolygon(resabs, false); 

				// sottrazione
				*pPolyBorder -= polyPunches;

				// verifica
				if (pPolyBorder->IsEmpty())
				{
					// Bordo completamente risolto.
					pBorder->SetResolved();
				}

				delete pPolyBorder;	
			}

			// Distruggo il bordo copia.
			pBorderCopy->DeleteAllItems();
			delete pBorderCopy;
		}
		else if (bOnlyArc) {
			pBorder->SetResolved();
		}
	}

	// Se il bordo e` una scantonatura (che a questo punto contiene qualche arco), devo 
	// controllare se ci sono segmenti della spezzata di sostituzione che si sovrappongono a 
	// segmenti del contorno piu` piccolo della scantonatura (ovvero si sovrappongono al 
	// bounding box del pannello). In tal caso entrambi i bordi della scantonatura 
	// vanno aggiornati.
	if (!pBorder->IsOriginal())
	{		
		// Bouding box della parte
		CbBox boxOfPart = this->GetPart()->GetBorder(0)->GetBBox();

		// Se il ricalcolo della scantonatura comporta che la scantonatura non ha
		// piu` ragione d'esistere, imposto il bordo come risolto.
		pBorder->JoinAlignedItems();
		if (!this->GetPart()->RecalcNotch(pBorder,
										  boxOfPart,
										  GetParameters().dMaxOverhangUp,
										  GetParameters().dMaxOverhangDown,
										  GetParameters().dMaxOverhangLeft,
										  GetParameters().dMaxOverhangRight,
										  GetParameters().dMinOverhang,
#ifdef COMPILING_CAMPUNCH_DLL
										  MAX_OVERHANG
#endif
#ifdef COMPILING_CAM_COMBI_DLL
										  COMBI_MAX_OVERHANG
#endif
										  
										  
										  ))
		{
			pBorder->SetResolved();
		}
		else
		{
			// verifica e corregge interferenza scantonatura con altre parti dello sheet (nest)
#ifdef COMPILING_CAMPUNCH_DLL
			this->GetPart()->GetSheet()->AdjustNotches(this->GetPart(), pBorder);
#endif
#ifdef COMPILING_CAM_COMBI_DLL
			this->GetPart()->AdjustNotches(pBorder);
#endif
		}
	}

	return 0;
}

//----------------------------------------------------------------------
//
// Description: Gestisce singolarmente gli archi di un bordo, punzonandoli e
//              sostituendoli con spezzate
//
// Arguments:
//   border - bordo da elaborare
//	 iToolType - tipo di utesile
//   bUseNibbling - uso utensile il roditura
//
//	Return: 0 = OK, -1 = aborted
//
// Remarks:	Gestisce singolarmente gli archi di un bordo, punzonandoli e
//          sostituendoli con spezzate
//
//----------------------------------------------------------------------
int 
CArcsBorderWorkStrategy::SingleArcMng(CAMBORDER &border, int iToolType, bool bUseNibbling)
{
	POSITION		pos,
					posArcInBorder;
	CgItem			*pCurItem;
	CbPoint			pInter;


	int iNumOfItems = border.CountItems();
	int iCurrCont = border.GetContour();
	bool bBorderOriginal = border.IsOriginal();

	// A. Maschi & M. Beninca 20/01/2002
	// Per ovviare al problema che gli ITEM adiacenti all'arco da modificare
	// possono stare sui contorni esterni (e quindi non possono essere cancellati)
	// cancello solo l'arco mentre le due rette vengono modificate

	// Faccio un prima scansione su tutti gli item del bordo per eliminare quegli archi che sono
	// smussi con raggio minore del parametro GetParameters().dMinRadSign, cioe` archi che
	// soddisfano contemporaneamente le seguente condizioni:
	// - angolo minore di 180 gradi;
	// - raggio dell'arco e` minore del parametro GetParameters().dMinRadSign
	// - i due item precedente e seguente sono rettilinei e tangenti all'arco

	// Tutti i controlli geometrici presuppongo l' uso degli item
	// del contour 1 (chiusura grande)
	bool bAbort = false;
	bool bRedo = true;
	while (!bAbort && bRedo) {

		bRedo = false;
		pCurItem = border.GetFirstItem();

		for (int i=0; i<border.CountItems() && !bAbort && !bRedo; i++)
		{
			if (WAIT_THREAD->IsStopped()) {
				bAbort = true;
				continue;
			}
			// Se il bordo e` una scantonatura ma non contiene piu' item originali
			// la scantonatura non ha piu` ragione d'esistere -> imposto il bordo come risolto
			if (!bBorderOriginal && border.CountOrigItems() == 0)
			{
				border.SetResolved();
				return 0;
			}
				
			if (pCurItem->GetType() == CgItem::TYPE_ARC)
			{
				CgArc  *pArc = (CgArc*) pCurItem;

				// Item precedente l'arco
				bool bPrevOri = true;
				CgItem *pPrevItem[2];
				pPrevItem[0] = border.GetPrevItem();
				if(!border.CurrItemIsOriginal() && !bBorderOriginal)
				{
					// se non e' parte del contorno originale considero 
					// le chiusure grande e piccola ossia i contour 1 e 0
					bPrevOri = false;
					border.SetContour(1);
					border.GetNextItem();
					pPrevItem[0] = border.GetPrevItem();
					border.SetContour(0);
					border.GetNextItem();
					pPrevItem[1] = border.GetPrevItem();
				}

				// Riprendo la posizione dell'arco
				border.GetNextItem();

				// Item seguente l'arco
				bool bNextOri = true;
				CgItem *pNextItem[2];
				pNextItem[0] = border.GetNextItem();
				if(!border.CurrItemIsOriginal() && !bBorderOriginal)
				{
					// se non e' parte del contorno originale considero 
					// le chiusure grande e piccola ossia i contour 1 e 0
					bNextOri = false;
					border.SetContour(1);
					border.GetPrevItem();
					pNextItem[0] = border.GetNextItem();
					border.SetContour(0);
					border.GetPrevItem();
					pNextItem[1] = border.GetNextItem();
				}

				// Riprendo la posizione dell'arco
				border.GetPrevItem();

				// condizioni necessarie perche' l'arco sia da sostituire (1/2)
				if (pArc->GetAngle() < M_PI && 
					IsLess(pArc->GetRadius(), GetParameters().dMinRadSign) &&
					pPrevItem[0]->GetType() == CgItem::TYPE_LINE &&
					pNextItem[0]->GetType() == CgItem::TYPE_LINE)
				{
					CgLine *pPrevLine = (CgLine *) pPrevItem[0];
					CgLine *pNextLine = (CgLine *) pNextItem[0];
					// condizioni necessarie perche' l'arco sia da sostituire (2/2)
					if (pPrevLine->GetDir() == pArc->GetStartTanVect() &&
						pNextLine->GetDir() == pArc->GetEndTanVect())
					{
						// Intersezione fra i due segmenti
						int iInters = Intersect(*pPrevLine, *pNextLine, pInter, false);
						ASSERT_EXC(iInters == 1);

						// Estremi dei due segmenti
						CbPoint p1 = pPrevLine->GetStart();
						CbPoint p2 = pNextLine->GetEnd();

						// Elimino l'arco (anche nel CAD)
						pArc->DeleteOnDestroy(true);
						border.DeleteCurrItem(true);
						// Ora position punta all'item successivo all'arco
						
						pNextLine->SetStart(pInter);
						if (!bNextOri)
						{
							// se non e' originale significa che ho appena modificato
							// il contour 1, quindi ora modifico il contour 0
							CgLine *pNextLine1 = (CgLine *) pNextItem[1];
							pNextLine1->SetStart(pInter);
						}

						pPrevLine->SetEnd(pInter);
						if (!bPrevOri)
						{
							// se non e' originale significa che ho appena modificato
							// il contour 1, quindi ora modifico il contour 0
							CgLine *pPrevLine1 = (CgLine *) pPrevItem[1];
							pPrevLine1->SetEnd(pInter);
						}

						// Siccome ho modificato direttamente gli ITEMs e il bordo
						// non lo sa forzo il ricalcolo dei dati
						border.RecalcAll();
						if (!bPrevOri || !bNextOri)
						{
							border.SetContour(0);
							border.RecalcAll();
							border.SetContour(1);
							border.RecalcAll();
						}

						bRedo = true;

						// Metto il position sul segmento che precedeva l'arco
						POSITION POS = border.FindItemPos(pPrevLine);
						if (POS != NULL)
							border.SetCurrItemPos(POS);
						else
							border.GetPrevItem();
					}
				}
			}
			pCurItem = border.GetNextItem();
		}
	}

	if (bAbort)
		return -1;

	if(!bBorderOriginal)
		border.SetContour(iCurrCont);

	// Creo una copia del bordo, che non verra` modificata: invece border verra`
	// modificato sostituendo gli archi con le spezzate.
	CgBorder borderOriginal(border);

	// Lista che conterra` gli archi del bordo, che alla fine saranno eliminati
	//CgItemList listArcs;

	iNumOfItems = border.CountItems();
	// Raccolgo tutti gli archi del bordo in un array, quindi li risolverero' in ordine
	// decrescente di raggio (in maniera da eventualmente saltare quelli gia' risolti 
	// dalle punzonature dei primi)
	CgItemArray aArcs;
	pCurItem = border.GetFirstItem();
	int i;
	for (i=0; i<iNumOfItems; i++)
	{
		if (pCurItem->GetType() == CgItem::TYPE_ARC)
		{
			CgArc		*pArc = (CgArc*) pCurItem;
			int iPos = aArcs.GetSize();
			for (int j = 0; j < aArcs.GetSize(); j++) {
				CgArc *pArc1 = (CgArc *)aArcs[j];
				if (IsGreater(pArc->GetRadius(), pArc1->GetRadius())) {
					// Devo inserirlo in posizione J
					iPos = j;
					break;
				}
			}
			aArcs.InsertAt(iPos, pCurItem);
		}
		pCurItem = border.GetNextItem();
	}


	// Loop sugli item del bordo: ogni arco viene punzonato e sostituito

	for (i=0; i<aArcs.GetSize() && !bAbort; i++) 
	{
		if (WAIT_THREAD->IsStopped()) {
			bAbort = true;
			continue;
		}
		pCurItem = aArcs[i];
		if (pCurItem->GetType() == CgItem::TYPE_ARC)
		{
			double			dMaxDist;
			CgBorder	borderSubs;
			CgArc			*pArc = (CgArc*) pCurItem;
			pos =			border.FindItemPos(pCurItem);

			// cerco arco identico nel bordo originale
			CgItem *pOrig = borderOriginal.GetFirstItem();
			bool bFound = false;
			int j;
			for (j=0; j<borderOriginal.CountItems() && !bFound; j++) {
				if (pOrig->GetType() == CgItem::TYPE_ARC) {
					CgArc *pArc1 = (CgArc*) pOrig;
					if (pArc->GetCenter() == pArc1->GetCenter() &&
						IsSameValue(pArc->GetRadius(), pArc1->GetRadius()) &&
						pArc->GetStart() == pArc1->GetStart() &&
						pArc->GetEnd() == pArc1->GetEnd()) {
						bFound = true;
						posArcInBorder = borderOriginal.GetCurrItemPos();
					}
				}
				pOrig = borderOriginal.GetNextItem();
			}
			ASSERT_EXC(bFound);

			// Aggiungo l'arco alla lista
			// listArcs.AddTail(pArc);

			// Creazione della spezzata di sostituzione
			if (CUtilsGeom::CreatePolylineSubs(borderOriginal, posArcInBorder, GetParameters().dIntOverlap, borderSubs, dMaxDist) == false)
			{
				// distruzione spezzata sostituzione
				borderSubs.DeleteAllItems();
				
				// creazione URA
				CmUra* pUra = new CmUra(&GETPUNCHPROGRAM, border.GetCenter());
				GETPUNCHPROGRAM.SAVE_PUNCH_OPERATION(pUra, &border);
				border.SetResolved(true);
				break;
			}

			// Assegno alla spezzata lo stesso layer dell'arco
			borderSubs.SetLayer(pArc->GetLayer());
						
			// Riprendo la posizione dell'arco
			border.SetCurrItemPos(pos);

			// Sostituzione dell'arco con la spezzata
			border.GetNextItem();
			POSITION POSTMP = border.GetCurrItemPos();
			border.GetPrevItem();
			border.ReplaceItem(&borderSubs, true);
			border.SetCurrItemPos(POSTMP);

			// Devo spostarmi indietro di un item perche` nel loop principale c'e` la
			// chiamata alla GetNextItem
			pCurItem = border.GetPrevItem();

			// Programmazione della punzonatura dell'arco. Se il bordo e` una scantonatura
			// faccio una prima elaborazione con il bordo piccolo in modo da minimizzare i
			// debordi e, se l'arco non e` risolto, una seconda elaborazione con il bordo grande.
			if (!border.IsOriginal())
			{
				border.SetContour(1);
				borderOriginal.SetContour(1);
			}
			PUNCH_INSTRUCTION_LIST lWIArc;
			CgToolArray aMountedTools;
			POSITION posLastSubs = border.GetCurrItemPos();

			int iRet = PunchArc(border, borderOriginal, borderSubs, posArcInBorder, posLastSubs, 
								dMaxDist, iToolType, bUseNibbling, lWIArc);
			if (iRet < 0)
				bAbort = true;

			// Aggancio le istruzioni individuate
			GETPUNCHPROGRAM.SAVE_PUNCH_OPERATIONS(&lWIArc, &border);
			lWIArc.RemoveAll();
			
			// Riaggancio l'arco nel bordo originale
			borderOriginal.SetCurrItemPos(posArcInBorder);
		}
		pCurItem = border.GetNextItem();
		borderOriginal.GetNextItem();
	}

	aArcs.DeleteAllItems();

	// Eliminazione definitiva degli archi
	//listArcs.DeleteAllItems();

	// Eliminazione del bordo copia
	borderOriginal.DeleteAllItems();

	return (bAbort ? -1 : 0);
}

//----------------------------------------------------------------------
//
// Description: Punzonatura di un cerchio
//
// Arguments:
//   borCircular - bordo costituito da un cerchio
//	 iToolType - tipo di utensile
//   bUseNibbling - uso utensile in roditura
//   bSaveScrap - flag di salvataggio sfrido
//   lWIArc - lista di istruzioni che vengono programmate per risolvere il cerchio
// Return value: true se risolto, false altrimenti
//
// Remarks:	Punzonatura di un cerchio
//
//----------------------------------------------------------------------
bool 
CArcsBorderWorkStrategy::PunchCircle(CAMBORDER &borCircular, int iToolType, bool bUseNibbling, bool bSaveScrap,
									  PUNCH_INSTRUCTION_LIST &lWIArc)
{
	CgCircle		*pCircle;
	int				iNumOfTools, i;
	CbPoint			c;
	double			dRad;
	// Puntatori a utensili
	CgToolCirc		*pToolCirc;
	CgToolRadMult4	*pToolRadMult4;
	CgToolRadMult8	*pToolRadMult8;
	CgToolTricusp	*pToolTricusp;
	double			dRoughness;

	// Il bordo deve essere un cerchio
	ASSERT_EXC(borCircular.IsCircle());

	if ((pCircle = (CgCircle *) borCircular.GetFirstItem()) == NULL)
		return false;
	c = pCircle->GetCenter();
	dRad = pCircle->GetRadius();

#ifdef COMPILING_CAMPUNCH_DLL
	if (dRad < GetParameters().dMinCircleRadSign) {
		return true;
	}
#endif

	// gestione approssimazione percentuale dell'utensile
	double dMinCirApp = GetParameters().dMinCirApp;
	if(GetParameters().bUsePercMinCirApp)
		dMinCirApp = dRad * GetParameters().dPercMinCirApp / 100.0;
	double dMaxCirApp = GetParameters().dMaxCirApp;
	if(GetParameters().bUsePercMaxCirApp)
		dMaxCirApp = dRad * GetParameters().dPercMaxCirApp / 100.0;

	// Controllo se esiste l'utensile esatto (e` considerato esatto se il suo raggio e` pari 
	// al raggio del foro a meno dei parametri di approssimazione massima superiore e inferiore).
	// Poiche` mi vengono dati gli utensili montati o montabili, se l'array non e` vuoto il primo
	// utensile va sicuramente bene
	CgToolArray aCirTools;
	iNumOfTools = GET_TOOL_DB.FindCirc(aCirTools, iToolType, CbCond(dRad, dMinCirApp, dMaxCirApp), true);
	for (i=0; i<iNumOfTools; i++)
	{
		pToolCirc = (CgToolCirc *)aCirTools[i];
		if (!CUtilsTools::IsToolUsableInPoint(GET_TOOL_DB, pToolCirc, c))
			continue;

		CmXyp *pXyp = new CmXyp(&GETPUNCHPROGRAM);
		pXyp->SetRefPoint(c);
		pXyp->SetUsedTool(pToolCirc);
		if (iToolType != CgTool::TYPE_PUNCH) {
			if (pToolCirc->GetEmbCycle()==1) 
				pXyp->GetOpt().SetPunOpt(PO_DR1);
			else if (pToolCirc->GetEmbCycle()==2) 
				pXyp->GetOpt().SetPunOpt(PO_DR2);
			else if (pToolCirc->GetEmbCycle()==3) 
				pXyp->GetOpt().SetPunOpt(PO_DR3);
			else if (pToolCirc->GetEmbCycle()==4) 
				pXyp->GetOpt().SetPunOpt(PO_DR4);
		}

		lWIArc.AddTail(pXyp);

		return true;
	}

	if (!bUseNibbling || iToolType != CgTool::TYPE_PUNCH)
		return false;

	// Se la punzonatura non e` risolta allora si deve impostare la roditura.
	// Continuo solo se la roditura e` compatibile con il tipo di utensile, se il raggio 
	// del cerchio e` maggiore del parametro dMinRadNIB e se e` maggiore di 2 volte lo spessore
	if (bUseNibbling && IsGreaterEqual(dRad, GetParameters().dMinRadNIB) && dRad >= 2.0 * GetParameters().dThickness)
	{
		// L'asperita` richiesta puo` essere espressa in termini assoluti o percentuali
		// sul raggio del cerchio
		if (GetParameters().bPercRoughNIB)
			dRoughness = dRad * GetParameters().dPercRoughNIB / 100.0;
		else
			dRoughness = GetParameters().dRoughNIB;

		// Cerco allora il miglior utensile per rodere e poi il migliore per la sgrossatura.

		bool			bToolExactFound = false;
		unsigned int	iBestToolNIB = NONE;
		double			dRadCirTool,
						dRadRadMult4Tool,
						dRadRadMult8Tool,
						dRadTricuspTool,
						dMinRoughness = DBL_MAX,
						dRoughToProg = DBL_MAX,
						dRoughCirTool = DBL_MAX,
						dRoughRadMult4Tool = DBL_MAX,
						dRoughRadMult8Tool = DBL_MAX,
						dRoughTricuspTool = DBL_MAX;
		int				iRad4Index, iRad8Index, iUnusedIndex;

		// Controllo se esiste il raggiatore multiplo (4 e 8 raggi) (che, se c'e`, e` sempre in posizione 
		// rotante) esatto o il tricuspide esatto. Se non e` diponibile nessuno dei due, prendo 
		// il migliore fra raggiatori multipli, tricuspidi (anche non rotanti) e circolari che 
		// garantisca di rispettare l'asperita` richiesta e abbia area di ingombro maggiore

		// Raggiatore multiplo (4 raggi)
		CgToolArray aRadMult4Tools; 
		iNumOfTools = GET_TOOL_DB.FindRadMult4(aRadMult4Tools, iToolType, CbCond(dRad, dMinCirApp, dMaxCirApp), true);
		if (iNumOfTools > 0)
		{
			pToolRadMult4 = (CgToolRadMult4 *)aRadMult4Tools[0];
			dRadRadMult4Tool = pToolRadMult4->GetNearestRadius(dRad);
			iRad4Index = pToolRadMult4->GetNearestIndex(dRad);
			int iRet = CUtilsTools::GetRealRoughForTool(&GET_TOOL_DB, pToolRadMult4, pCircle, dRoughness, dMinRoughness, iRad4Index);
			if (iRet == 0 || iRet == 3)
			{
				bToolExactFound = true;	
				// SDEX puo' ritornare asperita' negativa se il raggio usato per la roditura
				// e' maggiore del raggio dell'arco da rodere
				dRoughToProg = fabs(dMinRoughness);
				iBestToolNIB = MULT_RAD4;
			}
		}

		// Raggiatore multiplo (8 raggi)
		CgToolArray aRadMult8Tools; 
		iNumOfTools = GET_TOOL_DB.FindRadMult8(aRadMult8Tools, iToolType, CbCond(dRad, dMinCirApp, dMaxCirApp), true);
		if (iNumOfTools > 0)
		{
			pToolRadMult8 = (CgToolRadMult8 *)aRadMult8Tools[0];
			dRadRadMult8Tool = pToolRadMult8->GetNearestRadius(dRad);
			iRad8Index = pToolRadMult8->GetNearestIndex(dRad);
			int iRet = CUtilsTools::GetRealRoughForTool(&GET_TOOL_DB, pToolRadMult8, pCircle, dRoughness, dMinRoughness, iRad8Index);
			if (iRet == 0 || iRet == 3)
			{
				bToolExactFound = true;
				// SDEX puo' ritornare asperita' negativa se il raggio usato per la roditura
				// e' maggiore del raggio dell'arco da rodere
				dRoughToProg = fabs(dMinRoughness);
				iBestToolNIB = MULT_RAD8;
			}
		}
	
		// Tricuspide: se l'array non e` vuoto, controllo se il primo utensile ha 
		// il raggio esatto
		CgToolArray aTricuspTools;
		iNumOfTools = GET_TOOL_DB.FindTricusp(aTricuspTools, iToolType, CbCond(dRad, dMinCirApp, dMaxCirApp), true, false);
		if (iNumOfTools > 0)
		{
			pToolTricusp = (CgToolTricusp *)aTricuspTools[0];
			// Il raggio di ciascuna arco del tricuspide e` dato proprio dal parametro F,
			// indipendentemente dalla presenza o meno del parametro R.
			int iRet = CUtilsTools::GetRealRoughForTool(&GET_TOOL_DB, pToolTricusp, pCircle, dRoughness, dMinRoughness, iUnusedIndex);
			if (iRet == 0 || iRet == 3)
			{
				bToolExactFound = true;
				// SDEX puo' ritornare asperita' negativa se il raggio usato per la roditura
				// e' maggiore del raggio dell'arco da rodere
				dRoughToProg = fabs(dMinRoughness);
				iBestToolNIB = TRICUSP;
			}
		}


		// Se gli utensili esatti non sono stati trovati, trovo il migliore confrontando
		// le tre famiglie
		int iIndexRadMult4 = 0;
		int iIndexRadMult8 = 0;
		int iIndexTricusp = 0;
		int iIndexCirc = 0;
		bool bRadMult4Found = false;
		bool bRadMult8Found = false;
		bool bTricuspFound = false;
		bool bCircFound = false;
		int iNumPunches = INT_MAX;
		if (!bToolExactFound)
		{

			// Raggiatore multiplo 4 raggi: vanno bene non solo quelli che hanno raggio piu` piccolo
			// del raggio del cerchio da rodere ma anche quelli che hanno raggio maggiore, purche`
			// abbiano raggio di ingombro piu` piccolo di quello del cerchio e permettano
			// di avere una asperita` compatibile con quella richiesta
			iNumOfTools = GET_TOOL_DB.FindRadMult4(aRadMult4Tools, iToolType, COND_IGNORE, true);
			bRadMult4Found = iNumOfTools > 0;
			int iLoopInd;
			for (iLoopInd = 0; iLoopInd < iNumOfTools; iLoopInd++)
			{
				pToolRadMult4 = (CgToolRadMult4 *)aRadMult4Tools[iLoopInd];

				// Ignoro l'utensile se ha raggio di ingombro superiore al raggio
				// del cerchio da rodere
				if (IsGreater(pToolRadMult4->GetExtRadius(), dRad))
					continue;

				// Asperita` minima ottenibile (da SDEX)
				int iRet = CUtilsTools::GetRealRoughForTool(&GET_TOOL_DB, pToolRadMult4, pCircle, dRoughness, dMinRoughness, iRad4Index);
				dRadRadMult4Tool = pToolRadMult4->GetRadius(iRad4Index);
				// SDEX puo' ritornare asperita' negativa se il raggio usato per la roditura
				// e' maggiore del raggio dell'arco da rodere
				dMinRoughness = fabs(dMinRoughness);

				// In base alle impostazioni dell'utente la scelta fra utensili viene 
				// fatta tra quelli pi� grandi o quelli che producono l'asperit� minore
				bool bPrefer = false;
				if(!GetParameters().bPreferToolByRough)
					bPrefer = true;
				else
					bPrefer = IsLess(dMinRoughness, dRoughToProg);

				// Se c'e` compatibilita` di asperita` salvo l'area e il raggio per
				// poi eventualmente confrontarlo con i migliori fra i raggiatori a 8, i tricuspidi e
				// i circolari
				if (IsGreaterEqual(dRoughness, dMinRoughness) && bPrefer && (iRet == 0 || iRet == 3))
				{
					// Determino numero di copi necessari per rodere il cerchio 
					CmRic Ric(&GETPUNCHPROGRAM);
					Ric.SetRefPoint(c);
					Ric.SetUsedTool(pToolRadMult4);
					Ric.SetRadIndex(iRad4Index);
					if (GetParameters().bProgRadRough)
						Ric.SetWidth(dRad+dMinRoughness);
					else
						Ric.SetWidth(dRad);
					Ric.SetNibAngle(CbAngle(2.0*M_PI));
					Ric.SetSlope(CbAngle(0));
					Ric.SetRoughness(dMinRoughness);
					// questa chiamata trova il numero di colpi
					int iNum = Ric.GET_NUM_OF_PUNCH;
					// Espansione funziona?
					if(iNum <= 0)
						continue;
					// Conviene rispetto a quello che gia' ho?
					if (!GetParameters().bPreferToolByRough && iNum > iNumPunches)
						continue;

					dRoughRadMult4Tool = dMinRoughness;
					dRoughToProg = dRoughRadMult4Tool;
					iBestToolNIB = MULT_RAD4;
					iRad4Index = pToolRadMult4->GetNearestIndex(dRad);
					iIndexRadMult4 = iLoopInd;

					iNumPunches = iNum;

					// mi fermo subito se non devo scegliere l'utensile
					// che produce l'asperit� minore
					if(!GetParameters().bPreferToolByRough)
						break;
				}
			}

			// Raggiatore multiplo 8 raggi: vanno bene non solo quelli che hanno raggio piu` piccolo
			// del raggio del cerchio da rodere ma anche quelli che hanno raggio maggiore, purche`
			// abbiano raggio di ingombro piu` piccolo di quello del cerchio e permettano
			// di avere una asperita` compatibile con quella richiesta
			iNumOfTools = GET_TOOL_DB.FindRadMult8(aRadMult8Tools, iToolType, COND_IGNORE, true);
			bRadMult8Found = iNumOfTools > 0;
			for (iLoopInd = 0; iLoopInd < iNumOfTools; iLoopInd++)
			{
				pToolRadMult8 = (CgToolRadMult8 *)aRadMult8Tools[iLoopInd];

				// Ignoro l'utensile se ha raggio di ingombro superiore al raggio
				// del cerchio da rodere
				if (IsGreater(pToolRadMult8->GetExtRadius(), dRad))
					continue;

				// Asperita` minima ottenibile (da SDEX)
				int iRet = CUtilsTools::GetRealRoughForTool(&GET_TOOL_DB, pToolRadMult8, pCircle, dRoughness, dMinRoughness, iRad8Index);
				dRadRadMult8Tool = pToolRadMult8->GetRadius(iRad8Index);
				// SDEX puo' ritornare asperita' negativa se il raggio usato per la roditura
				// e' maggiore del raggio dell'arco da rodere
				dMinRoughness = fabs(dMinRoughness);

				// In base alle impostazioni dell'utente la scelta fra utensili viene 
				// fatta tra quelli pi� grandi o quelli che producono l'asperit� minore
				bool bPrefer = false;
				if(!GetParameters().bPreferToolByRough)
					bPrefer = true;
				else
					bPrefer = IsLess(dMinRoughness, dRoughToProg);

				// Se c'e` compatibilita` di asperita` salvo l'area e il raggio per
				// poi eventualmente confrontarlo con i migliori fra i raggiatori a 4, i tricuspidi e
				// i circolari
				if (IsGreaterEqual(dRoughness, dMinRoughness) && bPrefer && (iRet == 0 || iRet == 3))
				{
					// Determino numero di copi necessari per rodere il cerchio 
					CmRic Ric(&GETPUNCHPROGRAM);
					Ric.SetRefPoint(c);
					Ric.SetUsedTool(pToolRadMult8);
					Ric.SetRadIndex(iRad8Index);
					if (GetParameters().bProgRadRough)
						Ric.SetWidth(dRad+dMinRoughness);
					else
						Ric.SetWidth(dRad);
					Ric.SetNibAngle(CbAngle(2.0*M_PI));
					Ric.SetSlope(CbAngle(0));
					Ric.SetRoughness(dMinRoughness);
					// questa chiamata trova il numero di colpi
					int iNum = Ric.GET_NUM_OF_PUNCH;
					// Espansione funziona?
					if(iNum <= 0)
						continue;
					// Conviene rispetto a quello che gia' ho?
					if (!GetParameters().bPreferToolByRough && iNum > iNumPunches)
						continue;

					dRoughRadMult8Tool = dMinRoughness;
					dRoughToProg = dRoughRadMult8Tool;
					iBestToolNIB = MULT_RAD8;
					iRad8Index = pToolRadMult8->GetNearestIndex(dRad);
					iIndexRadMult8 = iLoopInd;

					iNumPunches = iNum;

					// mi fermo subito se non devo scegliere l'utensile
					// che produce l'asperit� minore
					if(!GetParameters().bPreferToolByRough)
						break;
				}
			}

			// Tricuspide
			iNumOfTools = GET_TOOL_DB.FindTricusp(aTricuspTools, iToolType, CbCond(dRad, dRad, 0), true, false);
			bTricuspFound = iNumOfTools > 0;
			if (iNumOfTools > 0)
			{
				pToolTricusp = (CgToolTricusp *)aTricuspTools[0];
				// Il raggio di ciascun arco del tricuspide e` dato proprio dal parametro F,
				// indipendentemente dalla presenza o meno del parametro R.
				dRadTricuspTool = pToolTricusp->GetRadius();

				// L'asperita` minima ottenibile (da SDEX)
				int iRet = CUtilsTools::GetRealRoughForTool(&GET_TOOL_DB, pToolTricusp, pCircle, dRoughness, dMinRoughness, iUnusedIndex);
				// SDEX puo' ritornare asperita' negativa se il raggio usato per la roditura
				// e' maggiore del raggio dell'arco da rodere
				dMinRoughness = fabs(dMinRoughness);

				// Se c'e` compatibilita` di asperita` e c'era anche per l'utensile
				// raggiatore multiplo, sceglio il migliore
				if (IsGreaterEqual(dRoughness, dMinRoughness) && (iRet == 0 || iRet == 3))
				{
					dRoughTricuspTool = dMinRoughness;

					// In base alle impostazioni dell'utente la scelta fra utensili viene 
					// fatta tra quelli pi� grandi o quelli che producono l'asperit� minore
					bool bPrefer = false;
					if(!GetParameters().bPreferToolByRough)
						bPrefer = true;
					else
						bPrefer = IsLess(dRoughTricuspTool, dRoughToProg);

					if (bPrefer)
					{
						// Determino numero di copi necessari per rodere il cerchio 
						CmRic Ric(&GETPUNCHPROGRAM);
						Ric.SetRefPoint(c);
						Ric.SetUsedTool(pToolTricusp);
						if (GetParameters().bProgRadRough)
							Ric.SetWidth(dRad+dMinRoughness);
						else
							Ric.SetWidth(dRad);
						Ric.SetNibAngle(CbAngle(2.0*M_PI));
						Ric.SetSlope(CbAngle(0));
						Ric.SetRoughness(dMinRoughness);
						// questa chiamata trova il numero di colpi
						int iNum = Ric.GET_NUM_OF_PUNCH;
						// Espansione funziona? Conviene rispetto a quello che gia' ho?
						if (iNum > 0 && (GetParameters().bPreferToolByRough || iNum < iNumPunches)) {
							dRoughToProg = dRoughTricuspTool;
							iBestToolNIB = TRICUSP;
							iIndexTricusp = 0;
							iNumPunches = iNum;
						}
					}
				}
			}
			// Circolare:
			// Calcolo il range di raggi entro cui cercare il circolare: il minimo raggio e`
			// pari al massimo fra il doppio dello spessore del foglio e il parametro che esprime
			// il minimo raggio dell'utensile per la roditura; il massimo raggio e` pari al minimo
			// fra 3/4 del raggio dell'arco da rodere (valore empirico scelto per evitare che, se
			// non c'e` l'utensile esatto, si utilizzi un utensile avente raggio troppo vicino a
			// quello del cerchio da rodere, in quanto cio` porterebbe a problemi tecnologici) e
			// il raggio del cerchio a cui sottraggo lo spessore del foglio (che implica diametro
			// meno 2 volte lo spessore)
			double	dMinRadToolSear = mymax(2.0 * GetParameters().dThickness, GetParameters().dMinToolNIB);
			double	dMaxRadToolSear = mymin(3.0 / 4.0 * dRad, dRad - GetParameters().dThickness);

			// Utensili disponibili che sono all'interno del range di raggi appena calcolato
			iNumOfTools = GET_TOOL_DB.FindCirc(aCirTools, iToolType, CbCond(dMaxRadToolSear, dMaxRadToolSear - dMinRadToolSear, 0), true);
			bCircFound = iNumOfTools > 0;

			// Se l'array non e` vuoto, o il primo utensile va bene come asperita` oppure
			// non se ne fa nulla
			if (iNumOfTools > 0)
			{
				for (int iTool = 0; iTool<iNumOfTools; iTool++) {
					pToolCirc = (CgToolCirc *)aCirTools[iTool];
					dRadCirTool = pToolCirc->GetRadius();

					// Asperit� minima ottenibile (da SDEX)
					int iRet = CUtilsTools::GetRealRoughForTool(&GET_TOOL_DB, pToolCirc, pCircle, dRoughness, dMinRoughness, iUnusedIndex);
					// SDEX puo' ritornare asperita' negativa se il raggio usato per la roditura
					// e' maggiore del raggio dell'arco da rodere
					dMinRoughness = fabs(dMinRoughness);

					if (IsGreaterEqual(dRoughness, dMinRoughness) && (iRet == 0 || iRet == 3))
					{
						dRoughCirTool = dMinRoughness;

						// In base alle impostazioni dell'utente la scelta fra utensili viene 
						// fatta tra quelli pi� grandi o quelli che producono l'asperit� minore
						bool bPrefer = false;

						// scelgo l'utensile in base alla sua grandezza oppure
						// alla asperit� prodotta
						if(!GetParameters().bPreferToolByRough)
							bPrefer = true;
						else
							bPrefer = IsLess(dRoughCirTool, dRoughToProg);

						if (bPrefer)
						{
							// Determino numero di copi necessari per rodere il cerchio 
							CmRic Ric(&GETPUNCHPROGRAM);
							Ric.SetRefPoint(c);
							Ric.SetUsedTool(pToolCirc);
							if (GetParameters().bProgRadRough)
								Ric.SetWidth(dRad+dMinRoughness);
							else
								Ric.SetWidth(dRad);
							Ric.SetNibAngle(CbAngle(2.0*M_PI));
							Ric.SetSlope(CbAngle(0));
							Ric.SetRoughness(dMinRoughness);
							// questa chiamata trova il numero di colpi
							int iNum = Ric.GET_NUM_OF_PUNCH;
							// Espansione funziona?
							if(iNum <= 0)
								continue;
							// Conviene rispetto a quelo che gia' ho?
							if (!GetParameters().bPreferToolByRough && iNum > iNumPunches)
								continue;
							iNumPunches = iNum;
							dRoughToProg = dRoughCirTool;
							iBestToolNIB = CIRCULAR;
							iIndexCirc = iTool;
						}
						// mi fermo subito se non devo scegliere l'utensile
						// che produce l'asperti� minore
						if(!GetParameters().bPreferToolByRough)
							break;
					}
				}
			}	
		}

		// popolo i dati per la parte seguente della funzione che deve conoscere
		// l'utensile usato per la finitura
		if(bRadMult4Found)
			pToolRadMult4 = (CgToolRadMult4*)aRadMult4Tools[iIndexRadMult4];
		if(bRadMult8Found)
			pToolRadMult8 = (CgToolRadMult8*)aRadMult8Tools[iIndexRadMult8];
		if(bTricuspFound)
			pToolTricusp = (CgToolTricusp*)aTricuspTools[iIndexTricusp];
		if(bCircFound)
			pToolCirc = (CgToolCirc*)aCirTools[iIndexCirc];

		// cerco il migliore (area maggiore) utensile per la sgrossatura, scegliendo fra i circolari e rettangolari
		if (iBestToolNIB != NONE)
		{
			int iBestToolSgr = NONE;
			double	dAreaCir  = 0.0, 
					dAreaRect = 0.0,
					dAreaRadMult4 = 0.0,
					dAreaRadMult8 = 0.0;
			CgToolCirc		*pToolCircSgr;
			CgToolRect		*pToolRectSgr;
			CgToolRadMult4	*pToolRadMult4Sgr;
			CgToolRadMult8	*pToolRadMult8Sgr;

			// Prendo il migliore circolare (per la sgrossatura)
			iNumOfTools = GET_TOOL_DB.FindCirc(aCirTools, iToolType, CbCond(dRad - dRoughToProg, dRad - dRoughToProg, 0), true);

			if (iNumOfTools > 0)
			{
				pToolCircSgr = (CgToolCirc *)aCirTools[0];
				dAreaCir = pToolCircSgr->GetArea();
			}

			// Cerco il migliore rettangolare (per la sgrossatura). Il piu` grande possibile
			// sara` il quadrato contenuto nel cerchio avente raggio pari a quello da distruggere
			// ridotto dell'asperita`, in quanto le punte dell'utensile rettangolare non 
			// possono (Sdex) entrare nella corona circolare identificata dall'asperita`
			CgToolArray aRectTools;
			iNumOfTools = GET_TOOL_DB.FindRect(aRectTools, iToolType, 
										  CbCond(2.0 * ((dRad - dRoughToProg) * sqrt(2.0) / 2.0), 2.0 * ((dRad - dRoughToProg) * sqrt(2.0) / 2.0), 0),
										  CbCond(2.0 * ((dRad - dRoughToProg) * sqrt(2.0) / 2.0), 2.0 * ((dRad - dRoughToProg) * sqrt(2.0) / 2.0), 0),
										  CbCond(),
										  true,
										  false);

			if (iNumOfTools > 0)
			{
				// Devo controllare che la freccia dell'arco identificata
				// dalla corda che ha lunghezza pari a ciascun lato del rettangolo,
				// sia minore dello spessore della corona circolare corrispondente
				// all'area effettivamente distrutta dall'utensile roditore (tale spessore
				// e` piu` piccolo del diametro dell'utensile (se circolare) in quanto
				// c'e` da tener conto dell'asperita`)
				// Inoltre Sdex accetta solo utensili sgrossatori che hanno inclinazione pari
				// a 0, 90, 180, 270.
				for (int iLoopInd = 0; iLoopInd < iNumOfTools; iLoopInd++)
				{
					pToolRectSgr = (CgToolRect *)aRectTools[iLoopInd];
					
					// M. Beninca 29/5/2002
					// Chiedo ad SDEX se � possibile eseguire la RIC con questo
					// utensile sgrossatore
					CmRic Ric(&GETPUNCHPROGRAM);
					Ric.SetRefPoint(c);
					switch (iBestToolNIB)
					{
						case MULT_RAD4:
							Ric.SetUsedTool(pToolRadMult4);
							Ric.SetRadIndex(iRad4Index);
							break;
						case MULT_RAD8:
							Ric.SetUsedTool(pToolRadMult8);
							Ric.SetRadIndex(iRad8Index);
							break;
						case TRICUSP:
							Ric.SetUsedTool(pToolTricusp);
							break;
						case CIRCULAR:
							Ric.SetUsedTool(pToolCirc);
					}
					Ric.SetUsedTool(pToolRectSgr, 1);
					if (GetParameters().bProgRadRough)
						Ric.SetWidth(dRad+dRoughness);
					else
						Ric.SetWidth(dRad);
					Ric.SetNibAngle(CbAngle(2.0*M_PI));
					Ric.SetSlope(CbAngle(0));
					Ric.SetRoughness(dRoughness);
					// questa chiamata trova il numero di colpi
					if(Ric.GET_NUM_OF_PUNCH > 0)
					{
						dAreaRect = pToolRectSgr->GetArea();
						break;
					}
				}
			}

			// Se per la roditura e` stato usato un utensile raggiatore multiplo (4/8),
			// per la sgrossatura e` possibile usare anche utensili raggiatori multipli (4/8):
			// cerco il migliore, per poi confrontarlo con gli eventuali altri
			// sgrossatori individuati
			if (iBestToolNIB == MULT_RAD4)
			{
				iNumOfTools = GET_TOOL_DB.FindRadMult4(aRadMult4Tools, iToolType, COND_IGNORE, true);
				for (int iLoopInd = 0; iLoopInd < iNumOfTools; iLoopInd++)
				{
					pToolRadMult4Sgr = (CgToolRadMult4 *)aRadMult4Tools[iLoopInd];

					// Va bene il primo utensile che ha raggio di ingombro inferiore
					// al raggio del cerchio da distruggere
					if (IsLessEqual(pToolRadMult4->GetExtRadius(), dRad))
					{
						// Come area dell'utensile considero l'area del quadrato
						// inscritto nel cerchio esterno dell'utensile (tale quadrato
						// ha i vertici coincidenti con i vertici degli archi
						// dell'utensile stesso)
						double dQuadSide = pToolRadMult4->GetExtRadius() / sin(M_PI / 4.0);
						dAreaRadMult4 = dQuadSide * dQuadSide;
						break;
					}

				}
			}
			else if (iBestToolNIB == MULT_RAD8)
			{
				iNumOfTools = GET_TOOL_DB.FindRadMult8(aRadMult8Tools, iToolType, COND_IGNORE, true);
				for (int iLoopInd = 0; iLoopInd < iNumOfTools; iLoopInd++)
				{
					pToolRadMult8Sgr = (CgToolRadMult8 *)aRadMult8Tools[iLoopInd];

					// Va bene il primo utensile che ha raggio di ingombro inferiore
					// al raggio del cerchio da distruggere
					if (IsLessEqual(pToolRadMult8->GetExtRadius(), dRad))
					{
						// Come area dell'utensile considero l'area del quadrato
						// inscritto nel cerchio esterno dell'utensile (tale quadrato
						// ha i vertici coincidenti con i vertici degli archi
						// dell'utensile stesso)
						double dQuadSide = pToolRadMult8->GetExtRadius() / sin(M_PI / 4.0);
						dAreaRadMult8 = dQuadSide * dQuadSide;
						break;
					}
				}
			}

			// Confronto gli utensili sgrossatori e monto il migliore. Se le aree sono
			// tutte nulle significa che non si e` trovato nessuno sgrossatore buono
			if (dAreaCir > 0.0 || dAreaRect > 0.0 || dAreaRadMult4 > 0.0 || dAreaRadMult8 > 0.0)
			{
				
				if (dAreaCir >= dAreaRect
				&&  dAreaCir >= dAreaRadMult4
				&&	dAreaCir >= dAreaRadMult8)
				{
					iBestToolSgr = CIRCULAR;
				}
				else if (dAreaRect >= dAreaCir
				&&		 dAreaRect >= dAreaRadMult4
				&&		 dAreaRect >= dAreaRadMult8)
				{
					iBestToolSgr = RECTANGULAR;	
				}
				else if (dAreaRadMult4 >= dAreaCir
				&&		 dAreaRadMult4 >= dAreaRect
				&&		 dAreaRadMult4 >= dAreaRadMult8)
				{
					iBestToolSgr = MULT_RAD4;
				}
				else
				{
					iBestToolSgr = MULT_RAD8;
				}
				
			}
			else
			{
				// Cerchio non risolto
				lWIArc.AddTail(new CmUra(&GETPUNCHPROGRAM, c));
				return false;
			}
			
			// Programmazione della punzonatura in base ai valori delle variabili
			// iBestToolNIB e iBestToolSgr.
			CmRic *pRic = new CmRic(&GETPUNCHPROGRAM);
			pRic->SetRefPoint(c);
			switch (iBestToolNIB)
			{
				case MULT_RAD4:
					pRic->SetUsedTool(pToolRadMult4);
					pRic->SetRadIndex(pToolRadMult4->GetNearestIndex(dRadRadMult4Tool));
					break;
				case MULT_RAD8:
					pRic->SetUsedTool(pToolRadMult8);
					pRic->SetRadIndex(pToolRadMult8->GetNearestIndex(dRadRadMult8Tool));
					break;
				case TRICUSP:
					pRic->SetUsedTool(pToolTricusp);
					break;
				case CIRCULAR:
					// attenzione la routine di risoluzione della parte interna
					// di una RIC in SDEX scazza (fa un sacco di colpi inutili) se
					// l'asperit� programmata � pi� grande del raggio dell'utensile
					// finitore. Quindi in questo caso la riduco tanto in ogni caso
					// l'asperit� reale sar� coerente con quella precedentemente calcolata
					if(dRoughness > pToolCirc->GetRadius())
						dRoughness = pToolCirc->GetRadius() - resabs;
					pRic->SetUsedTool(pToolCirc);
			}
			if (iBestToolSgr != NONE)
			{
				if (iBestToolSgr == CIRCULAR)
					pRic->SetUsedTool(pToolCircSgr, 1);
				else if (iBestToolSgr == RECTANGULAR)
					pRic->SetUsedTool(pToolRectSgr, 1);
				else if (iBestToolSgr == MULT_RAD4)
					pRic->SetUsedTool(pToolRadMult4Sgr, 1);
				else
					pRic->SetUsedTool(pToolRadMult8Sgr, 1);
			}
#if 0
			if (GetParameters().bProgRadRough)
				pRic->SetWidth(dRad+dRoughness);
			else
				pRic->SetWidth(dRad);
#else
			// Se uso 4/8 raggi con raggio identico non programmo raggio+asperita'
			if (!(GetParameters().bProgRadRough) ||
				(iBestToolNIB == MULT_RAD4 && IsSameValue(dRadRadMult4Tool, dRad)) ||
				(iBestToolNIB == MULT_RAD8 && IsSameValue(dRadRadMult8Tool, dRad))) 
				pRic->SetWidth(dRad);
			else
				pRic->SetWidth(dRad+dRoughness);
#endif
			pRic->SetRoughness(dRoughness);
			lWIArc.AddTail(pRic);

			return true;
		}
	}

	// Se siamo arrivati a questo punto, il cerchio non e` risolto
	lWIArc.AddTail(new CmUra(&GETPUNCHPROGRAM, c));

	return false;
}

//----------------------------------------------------------------------
//
// Description: Punzonatura di un arco orientato in senso orario
//
// Arguments:
//   border - bordo principale (modificato con la spezzata di sostituzione)
//   borderOrig - bordo originale con gli archi
//	 borderSubs - bordo sostituzione utilizzato
//   posArc - posizione dell'arco nel bordo originale
//   posLastSub - posizione, nel bordo modificato, dell'ultimo item 
//				  della spezzata di sostituzione
//   dMinDist - distanza minima di roditura
//	 iToolType - tipo di utensile
//   bUseNibbling - uso utensile in roditura
//
// Return value:
//	 1 se arco risolto completamente, 0 altrimenti, -1 aborted
//   lWIArc - lista di istruzioni che vengono programmate per risolvere l'arco
//
// Remarks:	Punzonatura di un arco
//
//----------------------------------------------------------------------
int				
CArcsBorderWorkStrategy::PunchClockwiseArc(CAMBORDER &border, CgBorder &borderOrig, CgBorder &borderSubs, POSITION posArc, 
							POSITION posLastSub, double dMinDist, int iToolType, bool bUseNibbling, 
							PUNCH_INSTRUCTION_LIST &lWIArc)
{
	// Puntatore all'arco
	CgArc			*pArc;
	// Puntatori agli items precedente e seguente
	CgItem			*pItemPrev,
					*pItemNext;
	// Puntatori a utensili
	CgToolCirc		*pToolCirc;
	CgToolRad		*pToolRad;
	CgToolRect		*pToolRect;
	// Punti di intersezione fra bordi
	CbPointArray	aPoints;
	// Centro e raggio dell'arco
	CbPoint			c, c1, c2;
	double			dRad, dRoughness, dEffRoughness, dRoughnessEnds;
	int				iNumOfTools, i, iBuffer;


	borderOrig.SetCurrItemPos(posArc);
	pArc = (CgArc *) borderOrig.GetCurrItem();
	c = pArc->GetCenter();
	dRad = pArc->GetRadius();
	pItemPrev = borderOrig.GetPrevItem();
	borderOrig.SetCurrItemPos(posArc);
	pItemNext = borderOrig.GetNextItem();

	// L'asperita` richiesta (sia per la NIB che agli estremi della NIB) puo` essere 
	// espressa in termini assoluti o percentuali sul raggio dell'arco
	if (GetParameters().bPercRoughNIB)
		dRoughness = dRad * GetParameters().dPercRoughNIB / 100.0;
	else
		dRoughness = GetParameters().dRoughNIB;
	if (GetParameters().bPercRoughNIBEnds)
		dRoughnessEnds = dRad * GetParameters().dPercRoughNIBEnds / 100.0;
	else
		dRoughnessEnds = GetParameters().dRoughNIBEnds;

	// Inizializzo l'asperita` effettiva ad un valore uguale a quella appena calcolata.
	// Tuttavia l'asperita` effettiva potra` essere poi inferiore quando il raggio
	// dell'utensile e` piccolo
	dEffRoughness = dRoughness;

	// gestione approssimazione percentuale dell'utensile
	double dMinCirApp = GetParameters().dMinCirApp;
	if(GetParameters().bUsePercMinCirApp)
		dMinCirApp = dRad * GetParameters().dPercMinCirApp / 100.0;
	double dMaxCirApp = GetParameters().dMaxCirApp;
	if(GetParameters().bUsePercMaxCirApp)
		dMaxCirApp = dRad * GetParameters().dPercMaxCirApp / 100.0;

	{
		// verifico se arco e' gia' risolto
		CgBorder borderToPunch;

		CgArc arc (c, dRad, pArc->GetEndVect(), - pArc->GetSignedAngle());
		borderToPunch.AddItem(&arc);
		borderToPunch.GetFirstItem();
		borderToPunch.SetAutoCheck(false);
		borderToPunch.InsertBorderAfter(&borderSubs);
		borderToPunch.SetAutoCheck(true);
		// Verifico se il bordo borderToPunch e' gia' stato risolto 
		bool bRet = CUtilsGeom::CheckBorderWithStoredPunches(&borderToPunch, &border, &borderOrig, GetPart(), iToolType);

		borderToPunch.Reset();
		if (bRet) {
			// Arco gia' eseguito
			return 1;
		}
	}

	if (WAIT_THREAD->IsStopped()) 
		return -1;

	// Arco deve essere orientato in senso orario
	if (pArc->GetSignedAngle() >= 0)
	{
		ASSERT(0);
		return -1;
	}

	bool bAbort = false;
	
	// Se l'arco e` tutto contenuto in un quadrante, controllo se esiste l'utensile raggiatore 
	// esatto (deve risolvere l'arco in un unico colpo)
	if (pArc->CrossingQuadrants() == 1)
	{
		// Quadrante su cui si sviluppa l'arco (il primo quadrante ha indice 0): 
		// lo calcolo in base all'angolo del punto medio dell'arco
		CbAngle angMid = CbVector(pArc->GetMid() - c).Angle();
		int iQuad = (int)floor(angMid.Get() / M_PI_2);
		iQuad++;
		CbPoint pointRealCenter(pArc->GetCenter()+CbVector(pArc->GetRadius()*sqrt(2.0),CbAngle(iQuad*M_PI_2-M_PI_2*0.5)));

		CgToolArray aToolsRad; 
		iNumOfTools = GET_TOOL_DB.FindRad(aToolsRad, iToolType, CbCond(), CbCond(dRad, dMinCirApp, dMaxCirApp), true);
		for (i=0; i<iNumOfTools && !bAbort; i++)
		{
			if (WAIT_THREAD->IsStopped()) {
				bAbort = true;
				continue;
			}
			pToolRad = (CgToolRad *)aToolsRad[i];

			// valori parametri I e H del raggiatore
			double dI = pToolRad->GetOblSide().x();
			double dH = pToolRad->GetOblSide().y();

			// Eseguo le stesse trasformazioni di SdeX per ottenere una XYP a partire dalla RAD.
			CbPoint pointXYP;
			{
				POINT_S CoorRel, CoorXYP, S_ActRef;
				double fPhaVet, S_ActRef_S;

				S_ActRef_S = 0.0;
				S_ActRef.x = 0.0;
				S_ActRef.y = 0.0;
				CoorXYP.x = pointRealCenter.x();
				CoorXYP.y = pointRealCenter.y();

				/* 
				 * Calcola l'offset del centro di tranciatura rispetto al punto programmato 
				 */
//				CoorRel.x = CoorRel.y = (pToolRad->GetSide() + dI)/2.0;
				CoorRel.x = CoorRel.y = (pToolRad->GetSide())/2.0;

				/* Determina la fase del vettore rispetto al sistema relativo */
				switch(iQuad){
					case 0:	  
					case 1:	fPhaVet = 000.; 	break; 
					case 2:	fPhaVet = 090.; 	break; 
					case 3:	fPhaVet = 180.; 	break;
					case 4:	fPhaVet = 270.; 	break; 
				}                      

				/* 
				 * Determina le coordinate relative del centro di tranciatura
				 */   
				CoorXYP = AssPoint(CoorRel, CoorXYP, fPhaVet);

				/* 
				 * Determina le coordinate assolute del centro di tranciatura 
				 */   
				CoorXYP = AssPoint(CoorXYP, S_ActRef, S_ActRef_S);
				pointXYP.SetX(CoorXYP.x);
				pointXYP.SetY(CoorXYP.y);
			}

			if (!CUtilsTools::IsToolUsableInPoint(GET_TOOL_DB, pToolRad, pointXYP))
				continue;

			// Dall'immagine del punzone, ricavo il bordo corrispondente
			CgImage *pImageTool = pToolRad->GetImage();
			if (pImageTool == NULL)
				continue;
			CgBorder *pBorTool = pImageTool->GetBorder(0);
			pBorTool->Move(pointXYP - CbPoint(0.0, 0.0, 0.0));
			// Modifica necessaria solo per nuova versione CAM3D::Offset
			CgBorder *pLittleTool = NULL;
			double dOff = -4.0*resabs;
			while (pLittleTool == NULL && -dOff > resabs*0.01) {
				dOff = dOff/2.0;
				pLittleTool = pBorTool->Offset(dOff);
			}
			if (pLittleTool == NULL)
				pLittleTool = new CgBorder(*pBorTool);

			bool bIsInside = false;
			int j = Intersect(*pLittleTool, borderOrig, aPoints);
			// Verifico se interseca non l'arco: infatti gli utensili raggiatori sono cosi' 
			// "sfigati" che per fare un'arco mangiano leggermente al suo interno (se specificati 
			// i parametri I e H)!
			for (j=0; j<aPoints.GetSize() && !bIsInside; j++) {
				CbPoint Pint(aPoints[j]);
				if (!pArc->Includes(Pint)) 
					bIsInside = true;
			}
			if (!bIsInside)
			{
				CmRad *pRad = new CmRad(&GETPUNCHPROGRAM);
				pRad->SetRefPoint(pointRealCenter);
				pRad->SetUsedTool(pToolRad);
				pRad->SetQuad(iQuad);
				lWIArc.AddTail(pRad);

				pLittleTool->DeleteAllItems();
				delete pLittleTool;
				pImageTool->DeleteAllItems();
				delete pImageTool;

				return 1;
			}

			// Distruggo oggetti creati per verifica RAD.
			pLittleTool->DeleteAllItems();
			delete pLittleTool;
			pImageTool->DeleteAllItems();
			delete pImageTool;
		}
	}

	if (bAbort)
		return -1;

	// Se la punzonatura non e` risolta allora si deve impostare la roditura.
	// Se il tipo di utensile richiesto non e` compatibile con la roditura oppure
	// se il raggio dell'arco e` minore del parametro dMinRadNIB oppure e` minore 
	// di 2 volte lo spessore, la punzonatura e` considerata non risolta
	if (!bUseNibbling || IsLess(dRad,GetParameters().dMinRadNIB) || IsLess(dRad,2.0 * GetParameters().dThickness))
	{
		// aree non risolte			
		CbPoint pointMid = pArc->GetMid();
		lWIArc.AddTail(new CmUra(&GETPUNCHPROGRAM, pointMid));

		return 0;
	}

	// Verifico se esiste utensile raggiatore multiplo concavo per eseguire l'arco.
	// Ricerca utensili raggiatori multipli concavi rotanti.

	CgToolArray aRadMultConcTools; 
	iNumOfTools = GET_TOOL_DB.FindRadMultConc(aRadMultConcTools, CgTool::TYPE_PUNCH, CbCond(1000.0,1000.0,0.0), true);

	int iNumHitRes = 0;
	CgTool *pToolRes = NULL;

#ifdef COMPILING_CAMPUNCH_DLL
	CCAD *pCurrCAD = pCamPunchCAD;
#endif
#ifdef COMPILING_CAM_COMBI_DLL
	CCAD *pCurrCAD = pCmbCadHndlr;
#endif
	for (i=0; i<iNumOfTools && !bAbort; i++) {
		if (WAIT_THREAD->IsStopped()) {
			bAbort = true;
			continue;
		}
		// L'utensile e' sicuramente montabile (perche' gia' montato essendo riconosciuto 
		// come rotante).
		CgTool *pTool = aRadMultConcTools[i];

		// Provo a programmare la punzonatura con una RES temporanea.
		CmRes *pRes = new CmRes(&GETPUNCHPROGRAM);

		pRes->SetRefPoint(pArc->GetCenter());
		pRes->SetUsedTool(pTool);
		pRes->SetWidth(dRad);
		pRes->SetRoughness(dRoughness);
		pRes->SetBridgeWidth(0.4/pCurrCAD->GetUnitScale());
		pRes->SetNumOfBridges(0);
		pRes->SetSlope(pArc->GetEndAngle());
		pRes->SetNibAngle(pArc->GetAngle());
		double dStartOverAng = DEG2RAD(1.0);
		double dEndOverAng = dStartOverAng;
		pRes->SetStartOverAng(dStartOverAng);
		pRes->SetEndOverAng(dEndOverAng);
		pRes->SetNibType(0);

		// Verifico se SdeX e' in grado di eseguire macro con parametri impostati.
		int nNumHit = pRes->GET_NUM_OF_PUNCH;
		if (nNumHit != 0) {
			if (!pRes->CheckVsBorder(&borderOrig)) {
				// Non e' possibile utilizzare la RES perche' deborda: provo ad eliminare i debordi.
				pRes->SetStartOverAng(0.0);
				pRes->SetEndOverAng(0.0);
				nNumHit = pRes->GET_NUM_OF_PUNCH;
				if (nNumHit != 0) {
					// Ri-check con il bordo.
					if (!pRes->CheckVsBorder(&borderOrig)) {
						nNumHit = 0;
					} else {
						pRes->Hide();
					}
				}
			} else {
				pRes->Hide();
			}
		}

		if (nNumHit == 0) {
			// La RES non e' proprio eseguibile per SdeX.
			delete pRes;
		} else {
		
			// Devo verificare se un giro con questo utensile basta.
			CgImage *pImaTool = pTool->GetImage();
			if (pImaTool == NULL) {
				// Qualcosa e' andato male: utensile non utilizzabile.
				delete pRes;
				continue;
			}
			CgBorder *pBorTool = pImaTool->GetBorder(0);
			CgToolRadMultConc *pToolRadMult = (CgToolRadMultConc *)pTool;
			// Spazzolo il bordo e per tutti gli archi non di raccordo verifico 
			// quale ha la distanza del centro dal centro dell'utensile, decrementata
			// del proprio raggio, minima: tale valore indica il raggio del centro inscritto
			// nell'utensile, e	quindi la distanza dall'arco sicuramente rosa interamente 
			// da un giro di RES.
			pBorTool->GotoFirstItem();
			double dRadCirInscritto = DBL_MAX;
			for (int j=0; j< pBorTool->CountItems(); j++) {
				CgItem *pItem = pBorTool->GetNextItem();
				if (pItem->GetType() == CgItem::TYPE_ARC) {
					CgArc *pToolArc = (CgArc *)pItem;
					if (!IsSameValue(pToolArc->GetRadius(), pToolRadMult->GetCornerRadius())) {
						// Arco di roditura, non di raccordo.
						dRadCirInscritto = mymin(dRadCirInscritto, 
							Distance(pToolArc->GetCenter(), 
									 CbPoint(pTool->GetShift().x(),pTool->GetShift().y()))-pToolArc->GetRadius());
					}
				}
			}
			pImaTool->DeleteAllItems();
			delete pImaTool; pImaTool = NULL;

			if (IsSameValue(dRadCirInscritto, DBL_MAX)) {
				// Qualcosa e' andato male: utensile non utilizzabile.
				delete pRes;
				continue;
			}

			dRadCirInscritto *= 2.0;		// Roditura con almeno il diametro del cerchio inscritto.
			dRadCirInscritto -= GetParameters().dIntOverlap;
			if (IsGreaterEqual(dRadCirInscritto, dMinDist)) {
				// Basta la RES.
			} else {
				// Dovrei programmare altre roditure.
				// Modifico da spezzata di sostituzione, dopo 
				// aver salvato il numero di items.
				int iNumItemsSubs = borderSubs.CountItems();
				ModPolylineForPOL(*pArc, borderSubs, dRadCirInscritto);
		
				// Quando la SubstituteArcs chiama la ArcPunching, la position nel
				// bordo modificato e` sull'ultimo item della spezzata che ha sostituito
				// l'arco. Ora sostituisco la spezzata ripristinando poi la position
				// correttamente.
				// Vado all'inizio della vecchia spezzata
				border.SetCurrItemPos(posLastSub);
				for (int k=0; k<iNumItemsSubs - 1; k++)
					border.GetPrevItem();
				border.ReplaceItems(iNumItemsSubs, &borderSubs, true);
				//delete pRes;
				//continue;
			}

			// Effettiva programmazione della RES.
			pToolRes = pTool;
			CmRes *pRealRes = new CmRes(&GETPUNCHPROGRAM);
			pRealRes->SetRefPoint(pRes->GetRefPoint());
			pRealRes->SetUsedTool(pTool);
			pRealRes->SetSlope(pRes->GetSlope());
			CbAngle AngNib(pRes->GetNibAngle());
			if (AngNib.Get()==CbAngle(0.0))
				pRealRes->SetNibAngle(2*M_PI);
			else
				pRealRes->SetNibAngle(AngNib);
			pRealRes->SetStartOverAng(pRes->GetStartOverAng());
			pRealRes->SetEndOverAng(pRes->GetEndOverAng());
			pRealRes->SetNibType(pRes->GetNibType());
			pRealRes->SetWidth(pRes->GetWidth());
			pRealRes->SetRoughness(pRes->GetRoughness());
			pRealRes->SetBridgeWidth(pRes->GetBridgeWidth());
			pRealRes->SetNumOfBridges(pRes->GetNumOfBridges());
			lWIArc.AddTail(pRealRes);
			iNumHitRes = nNumHit;
			break;
			//return 1;
		}
	}

	if (!bAbort && IsGreaterEqual(dRad,GetParameters().dRadRect)) {
		// Arco puo' essere eseguito con POL CEH di rettangoli per parametro utente.
		// Provo a impostare la roditura con utensile rettangolare solo se l'angolo che 
		// la tangente all'arco nell'estremo iniziale (finale) forma un angolo superiore 
		// a 90 gradi rispetto all'item precedente (seguente)
		CbAngle angWithPrev, angWithNext;
		if (pItemPrev->GetType() == CgItem::TYPE_ARC)
		{
			CbUnitVector unitV (((CgArc *)pItemPrev)->GetEndTanVect().Angle() + M_PI);
			angWithPrev = unitV.Angle() - pArc->GetStartTanVect().Angle();
		}
		else
			angWithPrev = (((CgLine *)pItemPrev)->GetDir().Angle() + M_PI) - pArc->GetStartTanVect().Angle();
		if (pItemNext->GetType() == CgItem::TYPE_ARC)
		{
			CbUnitVector unitV = ((CgArc *)pItemNext)->GetStartTanVect();
			angWithNext = (pArc->GetEndTanVect().Angle() + M_PI) - unitV.Angle();
		}
		else
			angWithNext = (pArc->GetEndTanVect().Angle() + M_PI) - (((CgLine *)pItemNext)->GetDir().Angle());
		
		if (angWithPrev >= M_PI_2 && angWithNext >= M_PI_2)
		{
			double		dWidthTool, dHeightTool, dDist;
			CbAngle		angOffset;
			CbPoint		centerTool;
			CbVector	vect1, vect2;
			CgLine		line1, line2;
			int			iNumOfPunches;
			CgBorder	*pBorToolOffset = NULL;

			double dRoughPol = GetParameters().dRoughRect;

			// Per il lato lungo dell'utensile i non pongo vincoli; per il
			// lato corto, la dimensione ottima e` data dalla distanza minima di roditura, 
			// il minimo deve essere il massimo fra 0 e 3 volte l'asperita` (valutazione
			// empirica) e il massimo e` infinito
			CgToolArray aRectTools; 
			iNumOfTools = GET_TOOL_DB.FindRect(aRectTools, iToolType, 
										  CbCond(),
										  CbCond(dMinDist, mymax(0.0, dMinDist - 3.0 * dRoughPol), COND_IGNORE),
										  CbCond(),
										  true,
										  true);

			for (i=0; i<iNumOfTools && !bAbort; i++)
			{
				if (WAIT_THREAD->IsStopped()) {
					bAbort = true;
					continue;
				}
				pToolRect = (CgToolRect *)aRectTools[i];
				// Costruisco bordo, simile a quello del tool, ma leggermente piu` piccolo (offset):
				// con questo bordo faro` i controlli di interferenza.
				CgImage *pImageTool = pToolRect->GetImage();
				if (pImageTool == NULL)
					continue;
				CgBorder *pBorderTool = pImageTool->GetBorder(0);
				if (pBorToolOffset)
				{
					pBorToolOffset->DeleteAllItems();
					delete pBorToolOffset;
					pBorToolOffset = NULL;
				}
				// Modifica necessaria solo per nuova versione CAM3D::Offset
				CgBorder *pBorToolOffset = NULL;
				double dOff = -4.0*CAMPUNCH_OFFSET_BORDER;
				while (pBorToolOffset == NULL && -dOff > resabs*0.01) {
					dOff = dOff/2.0;
					pBorToolOffset = pBorderTool->Offset(dOff);
				}
				if (pBorToolOffset == NULL)
					pBorToolOffset = new CgBorder(*pBorderTool);

				pImageTool->DeleteAllItems();
				delete pImageTool;

				angOffset = CbAngle(0.0);

				// Poiche` i punzoni restituiti dalla ricerca possono avere la base che
				// soddisfa le condizioni per la base e l'altezza che soddisfa quelle per
				// l'altezza, oppure la base (reale del punzone) che soddisfa le condizioni 
				// per l'altezza e l'altezza che soddisfa le condizioni per la base, devo
				// capire qual e` il lato di roditura (cioe` la mia base fittizia) e, se e`
				// diversa da quella reale, imposto un primo offset angolare di 90 gradi
				dWidthTool = pToolRect->GetWidth();
				dHeightTool = pToolRect->GetHeight();
				// Se entrambi i lati soddisfano le condizione dell'altezza prendo come
				// base il lato piu` lungo
				if (pToolRect->GetHeight() >= 3.0 * dRoughPol && pToolRect->GetWidth() >= 3.0 * dRoughPol)
				{
					if (pToolRect->GetWidth() < pToolRect->GetHeight())
					{
						dWidthTool = pToolRect->GetHeight();
						dHeightTool = pToolRect->GetWidth();
						angOffset = CbAngle(M_PI_2);
					}
				}
				// Se solo la l'altezza soddisfa le condizioni della base, giro l'utensile
				else if (pToolRect->GetHeight() >= 3.0 * dRoughPol)
				{
					dWidthTool = pToolRect->GetHeight();
					dHeightTool = pToolRect->GetWidth();
					angOffset = CbAngle(M_PI_2);
				}
				// Se solo la la base soddisfa le condizioni lascio tutto come �
				else if (pToolRect->GetWidth() >= 3.0 * dRoughPol)
				{
				}
				// se non soddisfo le condizioni non posso usare l'utensile
				// Questo dovrebbe essere gi� filtrato dalla ricerca ma per chiarezza!?
				else
					continue;
				
				// Dalla lunghezza del punzone calcolo il passo angolare massimo e l'asperita` 
				// massima. Il passo angolare lo calcolo considerando che affinche` sia garantito 
				// uno spessore di roditura superiore o uguale alla distanza minima di roditura, 
				// due colpi successivi devono sovrapporsi opportunamente, cioe` la relazione fra
				// lunghezza, altezza del punzone, raggio dell'arco e passo angolare deve essere:
				// lunghezza+sovrap. interna = 2*((dRad+altezza)*tan(angPitch/2)).
				// Confronto poi l'asperita` massima con il parametro di asperita` impostato; se
				// e` minore allora programmo con il passo angolare calcolato. Se invece e` maggiore,
				// ricalcolo il passo angolare tenendo conto dell'asperita` data come parametro
				CbAngle angPitch (2.0 * atan2((dWidthTool - GetParameters().dIntOverlap) / 2.0, dRad + dHeightTool));
				
				// l'arco sooteso � troppo piccolo: l'utensile non va bene!!!
				if(angPitch == CbAngle(0.0) || angPitch == M_PI_2)
					continue;

				double dRoughMax = dRad / cos((angPitch / 2.0).Get()) - dRad;
				if (dRoughMax < dRoughPol)
					dRoughPol = dRoughMax;
				else
					angPitch = 2.0 * acos(dRad / (dRad + dRoughPol));

				// Offset angolare del punzone (che e` dato dal proprio angolo piu` l'angolo 
				// di montaggio)
				angOffset = angOffset + pToolRect->GetTotalRotation();

				// Ruoto il bordo del punzone in modo che la base sia orizzontale
				pBorToolOffset->ZRotate(pBorToolOffset->GetCenter(), -CbSignedAngle(angOffset.Get()));

				// Affinche` non ci sia perdita di sfrido, fra la punzonatura i-esima e la 
				// i+2-esima non ci deve essere sovrapposizione. Questo significa che la base
				// dell'utensile deve essere al piu` 2*dRad*tan(angPitch). Tuttavia questo vincolo
				// sparisce se l'altezza del punzone e` superiore alla asperita`, perche` la
				// punzonatura i+1-esima risolve il problema dello sfrido
				if (IsLess(dHeightTool, dRoughPol) && IsGreater(dWidthTool, 2.0 * dRad * tan(angPitch.Get())))
					continue;

				// Cerco il modo di piazzare il primo e l'ultimo colpo: se la tangente all'arco 
				// nell'estremo iniziale (finale) forma un angolo superiore a 180 gradi rispetto 
				// all'item precedente (seguente), allora provo a piazzare il colpo iniziale 
				// (finale) in modo che l'estremo iniziale (finale) dell'arco coincida col punto 
				// medio del lato lungo dell'utensile e poi controllero` l'interferenza col resto 
				// del bordo originale. Se invece l'angolo e` minore di 180 gradi, l'utensile va 
				// piazzato in modo che l'estremo coincida con l'estremo dell'arco corrispondente
				CbUnitVector unitVStart = pArc->GetStartVect();
				CbSignedAngle angRot ((unitVStart.Angle() - CbAngle(M_PI_2)).Get());
				pBorToolOffset->ZRotate(pBorToolOffset->GetCenter(), angRot);
				centerTool = c + unitVStart * (dRad + dHeightTool / 2.0);
				pBorToolOffset->Move(centerTool - pBorToolOffset->GetCenter());
				if (angWithPrev >= M_PI)
				{
					dDist = dWidthTool / 2.0;

					// Se il primo colpo interseca il bordo originale, provo a spostare
					// l'utensile in base ai punti di intersezione con il bordo originale
					if (Intersect(*pBorToolOffset, borderOrig, aPoints) != 0)
					{
						// Per ognuno del punti di intersezione faccio passare una retta
						// ortogonale alla tangente all'arco nel primo estremo. La minima 
						// distanza dello start point dell'arco da ogni retta mi permette
						// di individuare di quanto deve spostarsi il punzone rispetto alla
						// posizione centrale impostata precedentemente
						dDist = Distance(pArc->GetStart(), aPoints[0]);
						for (int j=0; j<aPoints.GetSize(); j++)
						{
							CgInfLine infL (aPoints[j], unitVStart.Angle());
							dDist = mymin(dDist, Distance(pArc->GetStart(), infL));
						}
						dDist -= resabs;
						centerTool = pBorToolOffset->GetCenter() + pArc->GetStartTanVect() * (dWidthTool / 2.0 - dDist);
						pBorToolOffset->Move(centerTool - pBorToolOffset->GetCenter());

						// Se interseca ancora, passo direttamente all'utensile successivo, 
						if (Intersect(*pBorToolOffset, borderOrig, aPoints) != 0) {
							// Verifico possibilita' di piazzare colpo esattamente sull'estremo 
							// dell'arco.
							// continue;
							// Piazzo l'utensile in modo che il suo estremo coincida col primo estremo
							// dell'arco
							centerTool = pBorToolOffset->GetCenter() + pArc->GetStartTanVect() * (dDist);
							pBorToolOffset->Move(centerTool - pBorToolOffset->GetCenter());
							dDist = dWidthTool;
							// Se interseca ancora, passo all'utensile successivo.
							if (Intersect(*pBorToolOffset, borderOrig, aPoints) != 0)
								continue;
						}
					}
				}
				else
				{
					dDist = dWidthTool;

					// Piazzo l'utensile in modo che il suo estremo coincida col primo estremo
					// dell'arco
					centerTool = pBorToolOffset->GetCenter() + pArc->GetStartTanVect() * (dWidthTool / 2.0 + CAMPUNCH_OFFSET_BORDER);
					pBorToolOffset->Move(centerTool - pBorToolOffset->GetCenter());

					// Se il primo colpo interseca il bordo originale, passo direttamente
					// all'utensile successivo
					if (Intersect(*pBorToolOffset, borderOrig, aPoints) != 0)
						continue;
				}

				// Costruisco un line che parte dallo start point dell'arco e che coincide
				// con il lato dell'utensile che rode. Questa line servira` per vedere
				// se il primo e ultimo colpo si toccano ed eventualmente scongiurare
				// perdita di sfrido
				line1.SetStart(pArc->GetStart());
				line1.SetEnd(CbPoint(pArc->GetStart() + pArc->GetStartTanVect() * dDist));

				// Salvo la posizione del centro del primo colpo rispetto al centro dell'arco
				// (l'inclinazione dell'utensile e` data dalla tangente all'arco nel suo
				// punto iniziale)
				vect1 = pBorToolOffset->GetCenter() - c;

				// Ruoto il bordo il modo che abbia ancora la base orizzontale
				pBorToolOffset->ZRotate(pBorToolOffset->GetCenter(), -angRot);

				CbUnitVector unitVEnd = pArc->GetEndVect();
				angRot = CbSignedAngle((unitVEnd.Angle() - CbAngle(M_PI_2)).Get());
				pBorToolOffset->ZRotate(pBorToolOffset->GetCenter(), angRot);
				centerTool = c + unitVEnd * (dRad + dHeightTool / 2.0);
				pBorToolOffset->Move(centerTool - pBorToolOffset->GetCenter());
				if (angWithNext >= M_PI)
				{
					dDist = dWidthTool / 2.0;

					// Se l'ultimo colpo interseca il bordo originale, provo a spostare
					// l'utensile in base ai punti di intersezione con il bordo originale
					if (Intersect(*pBorToolOffset, borderOrig, aPoints) != 0)
					{
						// Per ognuno del punti di intersezione faccio passare una retta
						// ortogonale alla tangente all'arco nel secondo estremo. La minima 
						// distanza dell'end point dell'arco da ogni retta mi permette
						// di individuare di quanto deve spostarsi il punzone rispetto alla
						// posizione centrale impostata precedentemente
						dDist = Distance(pArc->GetEnd(), aPoints[0]);
						for (int j=0; j<aPoints.GetSize(); j++)
						{
							CgInfLine infL (aPoints[j], unitVEnd.Angle());
							dDist = mymin(dDist, Distance(pArc->GetEnd(), infL));
						}
						dDist -= resabs;
						centerTool = pBorToolOffset->GetCenter() - pArc->GetEndTanVect() * (dWidthTool / 2.0 - dDist);
						pBorToolOffset->Move(centerTool - pBorToolOffset->GetCenter());

						// Se interseca ancora, passo direttamente all'utensile successivo
						if (Intersect(*pBorToolOffset, borderOrig, aPoints) != 0) {
							// Verifico possibilita' di piazzare colpo esattamente sull'estremo 
							// dell'arco.
							// continue;

							// Piazzo l'utensile in modo che il suo estremo coincida col secondo estremo
							// dell'arco
							centerTool = pBorToolOffset->GetCenter() - pArc->GetEndTanVect() * (dDist);
							pBorToolOffset->Move(centerTool - pBorToolOffset->GetCenter());
							dDist = dWidthTool;

							// Se interseca ancora, passo direttamente all'utensile successivo
							if (Intersect(*pBorToolOffset, borderOrig, aPoints) != 0)
								continue;
						}
					}
				}
				else
				{
					dDist = dWidthTool;

					// Piazzo l'utensile in modo che il suo estremo coincida col secondo estremo
					// dell'arco
					centerTool = pBorToolOffset->GetCenter() - pArc->GetEndTanVect() * (dWidthTool / 2.0 + CAMPUNCH_OFFSET_BORDER);
					pBorToolOffset->Move(centerTool - pBorToolOffset->GetCenter());

					// Se l'ultimo colpo interseca il bordo originale, passo direttamente
					// all'utensile successivo
					if (Intersect(*pBorToolOffset, borderOrig, aPoints) != 0)
						continue;
				}

				// Costruisco un line che parte dallo start point dell'arco e che coincide
				// con il lato dell'utensile che rode. Questa line servira` per vedere
				// se il primo e ultimo colpo si toccano ed eventualmente scongiurare
				// perdita di sfrido
				line2.SetStart(pArc->GetEnd());
				line2.SetEnd(CbPoint(pArc->GetEnd() - pArc->GetEndTanVect() * dDist));

				// Salvo la posizione del centro dell'ultimo colpo rispetto al centro dell'arco
				// (l'inclinazione dell'utensile e` data dalla tangente all'arco nel suo
				// punto finale)
				vect2 = pBorToolOffset->GetCenter() - c;

				// Ora il primo e ultimo colpo non interferiscono col bordo originale. Devo
				// tuttavia controllare che, se i due colpi si toccano, non provochino
				// perdite di sfrido. Per questo guardo se l'eventuale punto di intersezione
				// di line1 e line2 ha distanza dal bordo piu` grande dell'altezza dell'utensile.
				// Se e` cosi` e tale distanza e` piu` piccola dell'asperita`, allora va bene perche`
				// non serve fare anche la POL. Se invece tale distanza e` piu` grande dell'altezza
				// dell'utensile e piu` grande dell'asperita`, devo cambiare utensile
				bool bPolIsNeeded = true;
				CbPoint pointInters;
				if (Intersect(line1, line2, pointInters, true) != 0)
				{
					if (Distance(pointInters, c) - dRad > dHeightTool && 
						Distance(pointInters, c) - dRad > dRoughPol)
						continue;
					else if (IsLessEqual(Distance(pointInters, c) - dRad,dRoughPol))
						bPolIsNeeded = false;
				}
				
				// Ruoto il bordo il modo che abbia ancora la base orizzontale
				pBorToolOffset->ZRotate(pBorToolOffset->GetCenter(), -angRot);

				// Se la POL deve essere fatta, controllo prima se primo e ultimo colpo singolo
				// sono abbastanza vicini da creare una situazione in cui un unico colpo al centro
				// dell'arco conclude la roditura

				bool bRESIsBetter = false;

				if (bPolIsNeeded && pArc->GetStartAngle() - pArc->GetEndAngle() < 2.0 * angPitch)
				{
					// Piazzo il bordo al centro dell'arco
					CbUnitVector unitVMid (pArc->GetEndAngle() + (pArc->GetStartAngle() - pArc->GetEndAngle()) / 2.0);
					angRot = CbSignedAngle((unitVMid.Angle() - CbAngle(M_PI_2)).Get());
					pBorToolOffset->ZRotate(pBorToolOffset->GetCenter(), angRot);
					centerTool = c + unitVMid * (dRad + dHeightTool / 2.0);
					pBorToolOffset->Move(centerTool - pBorToolOffset->GetCenter());

					// Se ci sono problemi di intersezioni, cambio direttamente utensile
					if (Intersect(*pBorToolOffset, borderOrig, aPoints) != 0)
						continue;
					else
					{
						// Programmo il colpo singolo centrale.
						// In questo caso la POL CEH ruchiedera' sicuramente tre colpi.
						// Memorizzo solo se conviene rispetto ad eventuale RES.

						if (iNumHitRes == 0 || iNumHitRes > 2) {
							// Conviene POL: elimino eventuale RES.
							if (iNumHitRes > 0) {
								lWIArc.RemoveHead();
#pragma CompileNote("secondo me qui c'e' memory leak")
								iNumHitRes = 0;
							}

							CmXyp *pXyp = new CmXyp(&GETPUNCHPROGRAM);
							pXyp->SetRefPoint(pBorToolOffset->GetCenter());
							pXyp->SetToolAngle(unitVMid.Angle() + M_PI_2 - angOffset);
							pXyp->SetUsedTool(pToolRect);
							lWIArc.AddTail(pXyp);

							bPolIsNeeded = false;
						} else {
							bRESIsBetter = true;
						}
					}
				}

				if (WAIT_THREAD->IsStopped()) {
					bAbort = true;
					continue;
				}
				if (bRESIsBetter)
					continue;
				
				// Il primo e ultimo colpo sono piazzabili. Se la POL non e` necessaria oppure se
				// lo e` ed e` costruibile senza interferenze col bordo originale, imposto la punzonatura. 
				CbAngle angEffPitch;
				if (!bPolIsNeeded ||
					(bPolIsNeeded && PolIsPossible(*pArc, 
												   *pBorToolOffset, 
												   dHeightTool, 
												   borderOrig, 
												   pArc->GetEndAngle() + angPitch, 
												   pArc->GetStartAngle() - angPitch, 
												   angPitch, 
												   angEffPitch, 
												   iNumOfPunches)))
				{

					// POL CEH identificata: conviene rispetto ad eventuale RES individuata?
					int iNumHitPol = iNumOfPunches + 2;
					if (!bPolIsNeeded)
						iNumHitPol ++;

					if (iNumHitRes != 0 && (iNumHitRes <= iNumHitPol || IsLess(dHeightTool, dMinDist))) {
						// Conviene RES
						continue;
					} else {
						// Conviene POL.
						// Elimino eventuale RES.
						if (iNumHitRes > 0) {
							lWIArc.RemoveHead();
#pragma CompileNote("secondo me qui c'e' memory leak")
							iNumHitRes = 0;
						}

						// Il primo e ultimo colpo li piazzo a parte, mentre la POL la faccio a
						// partire dal secondo fino al penultimo
						// Primo colpo (in corrispondenza dello start point dell'arco)
						CmXyp *pXyp1 = new CmXyp(&GETPUNCHPROGRAM);
						pXyp1->SetRefPoint(c + vect1);
						pXyp1->SetToolAngle(pArc->GetStartTanVect().Angle() - angOffset);
						pXyp1->SetUsedTool(pToolRect);
						lWIArc.AddTail(pXyp1);
						// Ultimo colpo (in corrispondenza dell'end point dell'arco)
						CmXyp *pXyp2 = new CmXyp(&GETPUNCHPROGRAM);
						pXyp2->SetRefPoint(c + vect2);
						pXyp2->SetToolAngle(pArc->GetEndTanVect().Angle() - angOffset);
						pXyp2->SetUsedTool(pToolRect);
						lWIArc.AddTail(pXyp2);

						// Programmazione della POL
						if (bPolIsNeeded)
						{
							CmPol *pPol = new CmPol(&GETPUNCHPROGRAM);
							pPol->SetRefPoint(c);
							pPol->SetUsedTool(pToolRect);
							pPol->SetToolAngle(pArc->GetEndAngle() - M_PI_2 - angOffset + angPitch);
							pPol->SetSlope(pArc->GetEndAngle() + angPitch);
							pPol->SetAngleBetween(angEffPitch);
							pPol->SetWidth(dRad + dHeightTool / 2.0);
							pPol->SetPunchNumber(iNumOfPunches);
							pPol->SetCEHStatus(true);
							lWIArc.AddTail(pPol);
						}

						// Se l'altezza dell'utensile e` piu` piccola della profondita`
						// dell'area da distruggere, modifico da spezzata di sostituzione, dopo 
						// aver salvato il numero di items.
						if (IsLess(dHeightTool, dMinDist))
						{
							int iNumItemsSubs = borderSubs.CountItems();
							ModPolylineForPOL(*pArc, borderSubs, dHeightTool);
		
							// Quando la SubstituteArcs chiama la ArcPunching, la position nel
							// bordo modificato e` sull'ultimo item della spezzata che ha sostituito
							// l'arco. Ora sostituisco la spezzata ripristinando poi la position
							// correttamente.
							border.SetCurrItemPos(posLastSub);
							// Vado all'inizio della vecchia spezzata
							for (int k=0; k<iNumItemsSubs - 1; k++)
								border.GetPrevItem();
							// Elimino la vecchia spezzata, item per item (ad ogni eliminazione il
							// puntatore di position si sposta sull'item successivo)
							border.ReplaceItems(iNumItemsSubs, &borderSubs, true);
						}

						// Arco risolto
						if (pBorToolOffset)
						{
							pBorToolOffset->DeleteAllItems();
							delete pBorToolOffset;
							pBorToolOffset = NULL;
						}
						return 1;
					}
				}
			}
			if (pBorToolOffset)
			{
				pBorToolOffset->DeleteAllItems();
				delete pBorToolOffset;
				pBorToolOffset = NULL;
			}
		}
	}

	if (bAbort)
		return -1;

	if (iNumHitRes != 0) {
		// Avevo individuato una RES, non superata da una POL CEH: la programmo.
		// In effetti essa e' gia' inserita in lWIArc
		return 1;
	}
	// Se non e` stato possibile programmare la POL, provo con l'utensile circolare.
	// Cerco il piu` piccolo utensile circolare che sia compatibile con i valori di asperita`
	// e i parametri impostati nell'interfaccia, e che mi permetta di risolvere il tutto
	// con un unico giro
	CgToolArray aCirTools; 
	double dRoughToProg = dRoughness;
	iNumOfTools = GET_TOOL_DB.FindCirc(aCirTools, iToolType, CbCond(mymax(dMinDist / 2.0 + GetConstants().dWidthCut, GetParameters().dMinToolNIB), 0.0, COND_IGNORE), true);
	for (i=0; i<iNumOfTools && !bAbort; i++)
	{
		if (WAIT_THREAD->IsStopped()) {
			bAbort = true;
			continue;
		}
		pToolCirc = (CgToolCirc *)aCirTools[i];
		double dRadTool = pToolCirc->GetRadius();

		// Controllo che l'asperita` non sia troppo grande per l'utensile scelto
		dEffRoughness = dRoughness;
		if (dEffRoughness > dRadTool*0.5)
			dEffRoughness = dRadTool*0.5;
		// Controllo se c'e` compatibilita` di asperita`
		if (NibbleIsPossible(*pArc, pItemPrev, pItemNext, pToolCirc, dEffRoughness, dRoughnessEnds, aPoints, iBuffer, dRoughToProg, true))
		{
			c1 = aPoints[0];
			c2 = aPoints[1];

			CbVector vecStart (c2 - c);
			CbVector vecEnd (c1 - c);
			// Controllo se la roditura interferisce col resto del bordo originale
			bool bNibOk = false;
			CbPolygon *pPolyNib = NULL;
			if (CUtilsGeom::CreatePolyNibbling(&GETPUNCHPROGRAM, pToolCirc, 0, c, c2, c1, dRad + 2.0 * dRadTool, dEffRoughness, -CAMPUNCH_OFFSET_BORDER, 
										pPolyNib) && pPolyNib != NULL)
			{
				// Controllo se interagisce col bordo originale
				CgBorder *pOffBor = NULL;
				double dOff = 2.0*resabs;
				while (pOffBor == NULL && dOff > resabs*0.01) {
					dOff = dOff/2.0;
					pOffBor = borderOrig.Offset(dOff);
				}
				if (pOffBor == NULL)
					pOffBor = new CgBorder(borderOrig);

				CbPolygon* pPolyBorder = pOffBor->GetPolygon(resabs, false);
				bNibOk = CUtilsGeom::IsNibblingContained(*pPolyNib, *pPolyBorder, 0.001);
				delete pPolyNib; pPolyNib = NULL;
				delete pPolyBorder;
				
				pOffBor->DeleteAllItems();
				delete pOffBor;
			}
			if (bNibOk) 
			{
				CmRic *pRic = new CmRic(&GETPUNCHPROGRAM);
				pRic->SetRefPoint(c);
				pRic->SetUsedTool(pToolCirc);
				pRic->SetSlope(vecStart.Angle());
				pRic->SetNibAngle(vecEnd.Angle() - vecStart.Angle());
				//if (GetParameters().bProgRadRough)
				//	pRic->SetWidth(dRad + 2.0 * dRadTool + dRoughToProg);
				//else
					pRic->SetWidth(dRad + 2.0 * dRadTool);
				pRic->SetRoughness(dRoughToProg);

				lWIArc.AddTail(pRic);

				// Arco risolto
				return 1;
			}
		}
	}

	// Se l'arco non e` risolto, cerco utensili piu` piccoli, e pero` sara`
	// necessario fare piu` giri di roditura

	unsigned int	iBestToolNIB = NONE;
	double			dRadTool, dThickNib;
	iNumOfTools = GET_TOOL_DB.FindCirc(aCirTools, iToolType, 
								  CbCond(mymax(dMinDist / 2.0 + GetConstants().dWidthCut, GetParameters().dMinToolNIB), 
										 mymax(dMinDist / 2.0 + GetConstants().dWidthCut, GetParameters().dMinToolNIB) - GetParameters().dMinToolNIB, 
										 0.0), true);
	for (i=0; i<iNumOfTools && !bAbort; i++)
	{
		if (WAIT_THREAD->IsStopped()) {
			bAbort = true;
			continue;
		}
		pToolCirc = (CgToolCirc *)aCirTools[i];
		dRadTool = pToolCirc->GetRadius();

		// Controllo che l'asperita` non sia troppo grande per l'utensile scelto
		dEffRoughness = dRoughness;
		if (dEffRoughness > dRadTool*0.5)
			dEffRoughness = dRadTool*0.5;
		// Controllo se c'e` compatibilita` di asperita`
		if (NibbleIsPossible(*pArc, pItemPrev, pItemNext, pToolCirc, dEffRoughness, dRoughnessEnds, aPoints, iBuffer, dRoughToProg, true))
		{
			// I punti li registro invertiti, in modo da ragionare "in senso antiorario"
			c1 = aPoints[1];
			c2 = aPoints[0];

			// Controllo se la roditura interferisce col resto del bordo originale
			CbVector vecStart (c1 - c);
			CbVector vecEnd (c2 - c);
			bool bNibOk = false;
			CbPolygon *pPolyNib = NULL;
			if (CUtilsGeom::CreatePolyNibbling(&GETPUNCHPROGRAM, pToolCirc, 0, c, c1, c2, dRad + 2.0 * dRadTool, dEffRoughness, -CAMPUNCH_OFFSET_BORDER, 
										pPolyNib) && pPolyNib != NULL)
			{
				// Controllo se interagisce col bordo originale
				CgBorder *pOffBor = NULL;
				double dOff = 2.0*resabs;
				while (pOffBor == NULL && dOff > resabs*0.01) {
					dOff = dOff/2.0;
					pOffBor = borderOrig.Offset(dOff);
				}
				if (pOffBor == NULL)
					pOffBor = new CgBorder(borderOrig);
				CbPolygon* pPolyBorder = pOffBor->GetPolygon(resabs, false);
				bNibOk = CUtilsGeom::IsNibblingContained(*pPolyNib, *pPolyBorder, 0.001);
				delete pPolyNib; pPolyNib = NULL;
				delete pPolyBorder;
				
				pOffBor->DeleteAllItems();
				delete pOffBor;
			}
			if (bNibOk) 
			{
				CmRic *pRic = new CmRic(&GETPUNCHPROGRAM);
				pRic->SetRefPoint(c);
				pRic->SetUsedTool(pToolCirc);
				pRic->SetSlope(vecStart.Angle());
				pRic->SetNibAngle(vecEnd.Angle() - vecStart.Angle());
				//if (GetParameters().bProgRadRough)
				//	pRic->SetWidth(dRad + 2.0 * dRadTool + dRoughToProg);
				//else
					pRic->SetWidth(dRad + 2.0 * dRadTool);
				pRic->SetRoughness(dRoughToProg);

				lWIArc.AddTail(pRic);

				iBestToolNIB = CIRCULAR;
				break;
			}
		}
	}

	// Se e` stato trovato il migliore utensile per la roditura dell'arco, imposto
	// i giri successivi per distruggere il resto.
	if (!bAbort && iBestToolNIB != NONE)
	{
		// Ampiezza della corona circolare coperta dalla prima roditura.
		dThickNib = 2.0 * dRadTool - dRoughToProg - GetConstants().dWidthCut;

		// Controllo se con la prima roditura e` gia` tutto risolto
		if (dThickNib >= dMinDist)
			return 1;

		// Raggio dell'arco ottenuto aumentando l'arco originale della ampiezza
		// della corona coperta dalla prima NIB. 
		double dRadIncr = dRad + dThickNib;

		// Controllo quante intersezioni ha la prima roditura con la spezzata di
		// sostituzione. Per questo considero l'arco che ha raggio pari a dRadRed
		// e ampiezza identificata dai centri iniziale e finale della roditura.
		// Il numero di intersezioni di questo arco con la spezzata di sostituzione
		// deve essere pari: se ha due (dovrebbero essere almeno due!) intersezioni, 
		// rimane un unico arco ridotto da rodere; se ha quattro intersezioni, gli 
		// archi rimanenti da rodere sono due, e cosi` via.
		// Individuato il numero di archi, si invoca una procedura ricorsiva
		// per ogni arco (ricorsiva perche` ogni arco puo` a sua volta portare
		// a considerare piu` archi)
		CgArc arcMaxNib (c, dRadIncr, pArc->GetEndVect(), CbSignedAngle(pArc->GetAngle().Get()));
		CgBorder borArc;
		borArc.AddItem(&arcMaxNib);
		int iInters = Intersect(borArc, borderSubs, aPoints);
		borArc.Reset();
		// Il numero di intersezioni deve essere pari oppure uno (in tal caso
		// la roditura ha completato a pelo la profondita` da rodere)
		//ASSERT_EXC(iInters == 1 || IsSameValue((double) iInters / 2.0, floor((double) iInters / 2.0)));

		// Ordinamento angolare dei punti lungo l'arco appena costruito
		CUtilsGeneral::SortByAngle(aPoints, c, pArc->GetEnd());

		// A questo punto chiamo una procedura che iterativamente distrugge
		// il resto dell'arco
		bool bTotallyResolved = true;
		for (int i=0; i>1, i<iInters; i+=2)
		{
			if (i+1>=iInters) {
				// aree non risolte
				lWIArc.AddTail(new CmUra(&GETPUNCHPROGRAM, aPoints[i]));
				bTotallyResolved = false;
			} else {
				CbVector v1 (aPoints[i] - c);
				CbVector v2 (aPoints[i+1] - c);
				if (!DoExternalNib(c, dRad, dRadIncr, v1, v2, border, borderOrig, dMinDist - dThickNib, 
								   iToolType, lWIArc))
				{
					// aree non risolte
					lWIArc.AddTail(new CmUra(&GETPUNCHPROGRAM, aPoints[i]));
					bTotallyResolved = false;
				}
			}
		}
		if (bTotallyResolved)
			return 1;
		else
			return 0;
	}
	else if (!bAbort) {
		// aree non risolte
		lWIArc.AddTail(new CmUra(&GETPUNCHPROGRAM, pArc->GetMid()));
		return 0;
	}
	
	return (bAbort ? -1 : 0);
}

//----------------------------------------------------------------------
//
// Description: Punzonatura di un arco orientato in senso antiorario
//
// Arguments:
//   border - bordo principale (modificato con la spezzata di sostituzione)
//   borderOrig - bordo originale con gli archi
//	 borderSubs - bordo sostituzione utilizzato
//   posArc - posizione dell'arco nel bordo originale
//   posLastSub - posizione, nel bordo modificato, dell'ultimo item 
//				  della spezzata di sostituzione
//   dMinDist - distanza minima di roditura
//	 iToolType - tipo di utensile
//   bUseNibbling - uso utensile in roditura
//
// Return value:
//	 1 se arco risolto completamente, 0 altrimenti, -1 aborted
//   lWIArc - lista di istruzioni che vengono programmate per risolvere l'arco
//
// Remarks:	Punzonatura di un arco
//
//----------------------------------------------------------------------
int	
CArcsBorderWorkStrategy::PunchAnticlockwiseArc(CAMBORDER &border, CgBorder &borderOrig, CgBorder &borderSubs, POSITION posArc, 
							POSITION posLastSub, double dMinDist, int iToolType, bool bUseNibbling, 
							PUNCH_INSTRUCTION_LIST &lWIArc)
{
	// Puntatore all'arco
	CgArc			*pArc;
	// Puntatori agli items precedente e seguente
	CgItem			*pItemPrev,
					*pItemNext;
	// Puntatori a utensili
	CgToolCirc		*pToolCirc;
	CgToolRadMult4	*pToolRadMult4;
	CgToolRadMult8	*pToolRadMult8;
	CgToolTricusp	*pToolTricusp;
	// Punti di intersezione fra bordi
	CbPointArray	aPoints;
	// Centro e raggio dell'arco
	CbPoint			c, c1, c2;
	double			dRad, dRadTool, dMultRad4Radius, dMultRad8Radius, 
					dRoughness, dEffRoughness, dRoughnessEnds, dMinRoughness;
	int				iNumOfTools, i, iBuffer;


	borderOrig.SetCurrItemPos(posArc);
	pArc = (CgArc *) borderOrig.GetCurrItem();


	if(0)
	{
		CgBorder tmp1Brd;
		tmp1Brd.AddItem(pArc);
		tmp1Brd.SetPen(4);
		tmp1Brd.DeleteOnDestroy(true);
		tmp1Brd.Show();
		tmp1Brd.Move(CbVector(200,200));
		pISM->WaitForUser();
	}

	c = pArc->GetCenter();
	dRad = pArc->GetRadius();
	pItemPrev = borderOrig.GetPrevItem();
	borderOrig.SetCurrItemPos(posArc);
	pItemNext = borderOrig.GetNextItem();

	//test foscarini
	if(0)
	{
		CgBorder tmp1Brd(border);
		tmp1Brd.SetPen(4);
		tmp1Brd.DeleteOnDestroy(true);
		tmp1Brd.Show();
		tmp1Brd.Move(CbVector(200,200));
		pISM->WaitForUser();
		//
		CgBorder tmp2Brd(borderOrig);
		tmp2Brd.SetPen(5);
		tmp2Brd.DeleteOnDestroy(true);
		tmp2Brd.Show();
		tmp2Brd.Move(CbVector(200,200));
		pISM->WaitForUser();
		//
		CgBorder tmp3Brd(borderSubs);
		tmp3Brd.SetPen(5);
		tmp3Brd.DeleteOnDestroy(true);
		tmp3Brd.Show();
		tmp3Brd.Move(CbVector(200,200));
		pISM->WaitForUser();


	}
	//test foscarini

	// L'asperita` richiesta (sia per la NIB che agli estremi della NIB) puo` essere 
	// espressa in termini assoluti o percentuali sul raggio dell'arco
	if (GetParameters().bPercRoughNIB)
		dRoughness = dRad * GetParameters().dPercRoughNIB / 100.0;
	else
		dRoughness = GetParameters().dRoughNIB;
	if (GetParameters().bPercRoughNIBEnds)
		dRoughnessEnds = dRad * GetParameters().dPercRoughNIBEnds / 100.0;
	else
		dRoughnessEnds = GetParameters().dRoughNIBEnds;

	// Inizializzo l'asperita` effettiva ad un valore uguale a quella appena calcolata.
	// Tuttavia l'asperita` effettiva potra` essere poi inferiore quando il raggio
	// dell'utensile e` piccolo
	dEffRoughness = dRoughness;

	// gestione approssimazione percentuale dell'utensile
	double dMinCirApp = GetParameters().dMinCirApp;
	if(GetParameters().bUsePercMinCirApp)
		dMinCirApp = dRad * GetParameters().dPercMinCirApp / 100.0;
	double dMaxCirApp = GetParameters().dMaxCirApp;
	if(GetParameters().bUsePercMaxCirApp)
		dMaxCirApp = dRad * GetParameters().dPercMaxCirApp / 100.0;

	{
		// verifico se arco e' gia' risolto
		CgBorder borderToPunch;

		CgArc arc (c, dRad, pArc->GetEndVect(), - pArc->GetSignedAngle());
		borderToPunch.AddItem(&arc);
		borderToPunch.GetFirstItem();
		borderToPunch.SetAutoCheck(false);
		borderToPunch.InsertBorderAfter(&borderSubs);
		borderToPunch.SetAutoCheck(true);
		// Verifico se il bordo borderToPunch e' gia' stato risolto 
		bool bRet = CUtilsGeom::CheckBorderWithStoredPunches(&borderToPunch, &border, &borderOrig, GetPart(), iToolType);

		borderToPunch.Reset();
		if (bRet) {
			// Arco gia' eseguito
			return 1;
		}
	}

	if (WAIT_THREAD->IsStopped()) {
		return -1;
	}


	// Arco deve essere orientato in senso antiorario
	if (pArc->GetSignedAngle() < 0)
	{
		ASSERT(0);
		return -1;
	}


	bool			bAbort = false;	
	bool			bToolExactFound = false;
	bool			bCompleteCircleIsPossible = false;
	unsigned int	iBestToolNIB = NONE;
	double			dRadCirTool,
					dRadRadMult4Tool,
					dRadRadMult8Tool,
					dRadTricuspTool,
					dRoughCirTool = DBL_MAX,
					dRoughRadMult4Tool = DBL_MAX,
					dRoughRadMult8Tool = DBL_MAX,
					dRoughTricuspTool = DBL_MAX;
	int				iRad4Index, iRad8Index;
	int				iNumPunchRad4 = INT_MAX,
					iNumPunchRad8 = INT_MAX,
					iNumPunchCir = INT_MAX,
					iNumPunchTricus = INT_MAX;
	CbAngle			angStartTricus, angNibTricus;

	// Costruisco il bordo compreso fra la spezzata di sostituzione e l'arco: tale bordo puo`
	// essere utile se l'arco sara` completamente non risolto, in modo da visualizzare
	// eventualmente la parte non risolta
	CgBorder borderToPunch;
	CgArc arc (c, dRad, pArc->GetEndVect(), - pArc->GetSignedAngle());
	borderToPunch.AddItem(&arc);
	borderToPunch.GetFirstItem();
	borderToPunch.SetAutoCheck(false);
	if(0)
		{
			CgBorder tmpImg(borderToPunch);
			tmpImg.SetPen(8);
			tmpImg.DeleteOnDestroy(true);
			tmpImg.Move(CbVector(200,200,0));
			tmpImg.Show();
			pISM->WaitForUser();
			CgBorder tmpImg1(borderSubs);
			tmpImg1.SetPen(7);
			tmpImg1.DeleteOnDestroy(true);
			tmpImg1.Move(CbVector(200,200,0));
			tmpImg1.Show();
			pISM->WaitForUser();





		}
	borderToPunch.InsertBorderAfter(&borderSubs);
	if(0)
		{
			CgBorder tmpImg(borderToPunch);
			tmpImg.SetPen(8);
			tmpImg.DeleteOnDestroy(true);
			tmpImg.Move(CbVector(400,400,0));
			tmpImg.Show();
			pISM->WaitForUser();

		}
	borderToPunch.SetAutoCheck(true);
	// Fintantoche' non viene visualizzato il bordo non risolto, memorizzo il suo centro
	// e lo distruggo.
	CbPoint ptCenterToPunch = borderToPunch.GetCenter();
	borderToPunch.Reset();

	// Verifico se e' possibile punzonare tutto il cerchio
	// Controllo se interagisce con il resto del bordo: per il controllo, riduco il
	// raggio dell'utensile circolare di un po' rispetto al raggio dell'arco
	
	// se l'arco e' esageratamente piccolo non posso di certo risolverlo
	if (dRad - CAMPUNCH_OFFSET_BORDER <= 0.0)
	{
		lWIArc.AddTail(new CmUra(&GETPUNCHPROGRAM, pArc->GetMid()));
		return 0;
	}
	
	CgCircle cirTool (c, dRad - CAMPUNCH_OFFSET_BORDER);
	CgBorder borTool;
	borTool.AddItem(&cirTool);
	int nNumInt = Intersect(borTool, borderOrig, aPoints);
	if (nNumInt == 0)
		bCompleteCircleIsPossible = true;

	CgToolArray aCirTools; 
	if (bCompleteCircleIsPossible) {
		// Controllo se esiste l'utensile esatto (e` considerato esatto se il suo raggio e` pari 
		// al raggio del foro a meno dei parametri di approssimazione massima superiore e inferiore).
		// La ricerca viene fatta inizialmente fra i circolari, poi fra i raggiatori multipli e
		// tricuspidi rotanti. In questi ultimi due casi, oltre all'uguaglianza del raggio
		// l'ampiezza angolare dell'arco deve essere minore di quella dell'utensile.
		iNumOfTools = GET_TOOL_DB.FindCirc(aCirTools, iToolType, CbCond(dRad, dMinCirApp, dMaxCirApp), true);
		for (i=0; i<iNumOfTools && !bAbort; i++)
		{
			if (WAIT_THREAD->IsStopped()) {
				bAbort = true;
				continue;
			}
			pToolCirc = (CgToolCirc *)aCirTools[i];
			if (!CUtilsTools::IsToolUsableInPoint(GET_TOOL_DB, pToolCirc, c))
				continue;
			// Controllo se interagisce con il resto del bordo: per il controllo, riduco il
			// raggio dell'utensile circolare di un po' rispetto al raggio dell'arco
			CgCircle cirTool (c, mymin(pToolCirc->GetRadius(), dRad - CAMPUNCH_OFFSET_BORDER));
			CgBorder borTool;
			borTool.AddItem(&cirTool);
			int nNumInt = Intersect(borTool, borderOrig, aPoints);
			borTool.Reset();
			if (nNumInt == 0)
			{
				// Verifico se una stessa punzonatura e' gia' stata programmata 
				// (caso di bordi con due archi appartenenti allo stesso cerchio).
				// Statement array associato al bordo
				PUNCH_INSTRUCTION_LIST* pWIList = border.GET_PUNCH_OPERATIONS;

				if (pWIList->IsEmpty() == false) 
				{
					for (POSITION pos = pWIList->GetHeadPosition(); pos != NULL;)
					{
						PUNCH_INSTRUCTION *pWI = pWIList->GetNext(pos);
						if (pWI == NULL)
							continue;

						// Salto le istruzioni non di tipo XYP 
						if (pWI->GetType() != PUNCH_INSTRUCTION::WORK_XYP)
							continue;

						// Verifico se esiguita con utensile del tipo richiesto.
						CmXyp *pXyp = (CmXyp*)pWI;
						CgTool *pTool = pXyp->GetUsedTool();
						if (pTool != pToolCirc)
							continue;
						CbPoint Pc(pXyp->GetRefPoint().x(),pXyp->GetRefPoint().y(),c.z());
						if (Pc != c)
							continue;

						// Individuato punzonatura identica: esco senza programmarla di nuovo.
						return 1;
					}
				}

				// Programmazione della punzonatura
				CmXyp *pXyp = new CmXyp(&GETPUNCHPROGRAM);
				pXyp->SetRefPoint(c);
				pXyp->SetUsedTool(pToolCirc);
				lWIArc.AddTail(pXyp);
				return 1;
			}
		}
	}

	if (bAbort)
		return -1;

	// Se la punzonatura non e` risolta allora si deve impostare la roditura.
	// Se il tipo di utensile richiesto non e` compatibile con la roditura oppure
	// se il raggio dell'arco e` minore del parametro dMinRadNIB oppure e` minore 
	// di 2 volte lo spessore, la punzonatura e` considerata non risolta
	if (!bUseNibbling || IsLess(dRad, GetParameters().dMinRadNIB) || dRad < 2.0 * GetParameters().dThickness)
	{
		// aree non risolte
		lWIArc.AddTail(new CmUra(&GETPUNCHPROGRAM, ptCenterToPunch));
		return 0;
	}
	
	int iOldStmtNum = lWIArc.GetCount();

	// Controllo se esiste il raggiatore multiplo (che, se c'e`, e` sempre in posizione 
	// rotante) esatto. Se non e` diponibile, prendo il migliore fra raggiatori multipli
	// e circolari che garantisca di rispettare l'asperita` richiesta e abbia area maggiore.
	// I tricuspidi li escludo in quanto si possono usare solo per multipli di 60 gradi
	// (vedi istruzione RIC)

	// Raggiatore multiplo 4 raggi
	CgToolArray aRadMult4Tools; 
	double dRoughToProg = DBL_MAX;
	iNumOfTools = GET_TOOL_DB.FindRadMult4(aRadMult4Tools, iToolType, CbCond(dRad, dMinCirApp, dMaxCirApp), true);
	for (i=0; i<iNumOfTools && !bAbort; i++)
	{
		if (WAIT_THREAD->IsStopped()) {
			bAbort = true;
			continue;
		}
		pToolRadMult4 = (CgToolRadMult4 *)aRadMult4Tools[i];

		dEffRoughness = dRoughness;
		// L'utensile deve essere compatibile con le asperita`
		if (NibbleIsPossible(*pArc, pItemPrev, pItemNext, pToolRadMult4, dEffRoughness, dRoughnessEnds, aPoints, iRad4Index, dRoughToProg, true))
		{
			c1 = aPoints[0];
			c2 = aPoints[1];
			CbPolygon* pPolyNib = NULL;
			bool bToolOk = false;
			if (CUtilsGeom::CreatePolyNibbling(&GETPUNCHPROGRAM, pToolRadMult4, iRad4Index, c, c1, c2, dRad, dEffRoughness, -CAMPUNCH_OFFSET_BORDER, 
										pPolyNib) && pPolyNib != NULL)
			{
				// Controllo se interagisce col bordo originale
				CgBorder *pOffBor = NULL;
				double dOff = 2.0*resabs;
				while (pOffBor == NULL && dOff > resabs*0.01) {
					dOff = dOff/2.0;
					pOffBor = borderOrig.Offset(dOff);
				}
				if (pOffBor == NULL)
					pOffBor = new CgBorder(borderOrig);
				CbPolygon *pPolyBorder = pOffBor->GetPolygon(resabs, false);
				bToolOk = CUtilsGeom::IsNibblingContained(*pPolyNib, *pPolyBorder, 0.003);
				delete pPolyNib; pPolyNib = NULL;
				delete pPolyBorder;
				pOffBor->DeleteAllItems();
				delete pOffBor;
			}
			if(bToolOk){
				bToolExactFound = true;
				dRadTool = pToolRadMult4->GetExtRadius();
				dMultRad4Radius = pToolRadMult4->GetRadius(iRad4Index);
				iBestToolNIB = MULT_RAD4;
				break;
			}
		}
	}

	CgToolArray aRadMult8Tools; 
	if (!bToolExactFound && !bAbort)
	{
		// Raggiatore multiplo 8 raggi
		iNumOfTools = GET_TOOL_DB.FindRadMult8(aRadMult8Tools, iToolType, CbCond(dRad, dMinCirApp, dMaxCirApp), true);
		for (i=0; i<iNumOfTools && !bAbort; i++)
		{
			if (WAIT_THREAD->IsStopped()) {
				bAbort = true;
				continue;
			}
			pToolRadMult8 = (CgToolRadMult8 *)aRadMult8Tools[i];

			dEffRoughness = dRoughness;
			// L'utensile deve essere compatibile con le asperita`
			if (NibbleIsPossible(*pArc, pItemPrev, pItemNext, pToolRadMult8, dEffRoughness, dRoughnessEnds, aPoints, iRad8Index, dRoughToProg, true))
			{
				c1 = aPoints[0];
				c2 = aPoints[1];
				CbPolygon *pPolyNib = NULL;
				bool bToolOk = false;
				if (CUtilsGeom::CreatePolyNibbling(&GETPUNCHPROGRAM, pToolRadMult8, iRad8Index, c, c1, c2, dRad, dEffRoughness, -CAMPUNCH_OFFSET_BORDER, 
											pPolyNib) && pPolyNib != NULL)
				{
					// Controllo se interagisce col bordo originale
					CgBorder *pOffBor = NULL;
					double dOff = 2.0*resabs;
					while (pOffBor == NULL && dOff > resabs*0.01) {
						dOff = dOff/2.0;
						pOffBor = borderOrig.Offset(dOff);
					}
					if (pOffBor == NULL)
						pOffBor = new CgBorder(borderOrig);
					CbPolygon* pPolyBorder = pOffBor->GetPolygon(resabs, false);
					bToolOk = CUtilsGeom::IsNibblingContained(*pPolyNib, *pPolyBorder, 0.003);
					delete pPolyNib; pPolyNib = NULL;
					delete pPolyBorder;
					pOffBor->DeleteAllItems();
					delete pOffBor;
				}
				if(bToolOk){
					bToolExactFound = true;
					dRadTool = pToolRadMult8->GetExtRadius();
					dMultRad8Radius = pToolRadMult8->GetRadius(iRad8Index);
					iBestToolNIB = MULT_RAD8;
					break;
				}
			}
		}
	}


	// Se gli utensili esatti non sono stati trovati, cerco fra i raggiatori multipli  e
	// i circolari il migliore utensile per la roditura. I tricuspidi li escludo in 
	// quanto si possono usare solo per multipli di 60 gradi (vedi istruzione RIC)
	if (!bToolExactFound && !bAbort)
	{
		dMinRoughness = dRoughness;

		// Arrays di utensili per ogni famiglia, ordinato in modo decrescente.
		// Raggiatore multiplo 4 raggi
		int iNumRadMult4 = GET_TOOL_DB.FindRadMult4(aRadMult4Tools, iToolType, CbCond(dRad, dRad, 0), true);
		// Raggiatore multiplo 8 raggi
		int iNumRadMult8 = GET_TOOL_DB.FindRadMult8(aRadMult8Tools, iToolType, CbCond(dRad, dRad, 0), true);

		// Circolare:
		// Calcolo il range di raggi entro cui cercare il circolare: il minimo raggio e`
		// pari al massimo fra il doppio dello spessore del foglio e il parametro che esprime
		// il minimo raggio dell'utensile per la roditura; il massimo raggio e` pari al minimo
		// fra 3/4 del raggio dell'arco da rodere (valore empirico scelto per evitare che, se
		// non c'e` l'utensile esatto, si utilizzi un utensile avente raggio troppo vicino a
		// quello del cerchio da rodere, in quanto cio` porterebbe a problemi tecnologici) e
		// il raggio del cerchio a cui sottraggo lo spessore del foglio (che implica diametro
		// meno 2 volte lo spessore)
		double	dMinRadToolSear = mymax(2.0 * GetParameters().dThickness, GetParameters().dMinToolNIB);
		// M. Beninca 20/3/2002
		// il raggio minimo non pu� essere pi� grande della minina sovrapposizione tra due
		// punzonature / 2. In questo caso non avrei roditura!
		dMinRadToolSear = mymax(dMinRadToolSear, GetConstants().dMinDistCirRough / 2.0);
		double	dMaxRadToolSear = mymin(3.0 / 4.0 * dRad, dRad - GetParameters().dThickness);
		// Utensili disponibili che sono all'interno del range di raggi appena calcolato
		int iNumCircular = GET_TOOL_DB.FindCirc(aCirTools, iToolType, CbCond(dMaxRadToolSear, dMaxRadToolSear - dMinRadToolSear, 0), true);

		// Tricuspide
		CgToolArray aTricuspTools; 
		int iNumTricuspidal = GET_TOOL_DB.FindTricusp(aTricuspTools, iToolType, CbCond(dRad, dRad, 0), true, false);

		int iMaxTools = (int) __max(iNumRadMult4, iNumCircular);
		iMaxTools = (int) __max(iMaxTools, iNumRadMult8);
		iMaxTools = (int) __max(iMaxTools, iNumTricuspidal);

		// Loop per trovare il migliore utensile: il loop e` necessario in quanto se si trova
		// un utensile compatibile con le asperita` ma che interferisce col bordo originale,
		// si prova con un utensile piu` piccolo
		bool bStop = false;
		bool bPreferToolByRough = GetParameters().bPreferToolByRough;

		// M. Beninca 29/5/2002
		// Eseguo due volte il loop se devo scegliere l'utensile che produce l'asperit� minore
		// La prima volta cerco su tutti gli utensili quello che produce l'asperit�
		// pi� piccola. La seconda se non ho successo provo con l'utensile pi� grande

		int iIndexRad4Found = 0;
		int iIndexRad8Found = 0;
		int iIndexCircFound = 0;
		int iIndexTricusFound = 0;
		int iNumCircFound = iNumCircular;
		int iNumRadMult4Found = iNumRadMult4;
		int iNumRadMult8Found = iNumRadMult8;
		int iNumTricusFound = iNumTricuspidal;
		bool bCircFound = iNumCircular > 0;
		bool bRadMult4Found = iNumRadMult4 > 0;
		bool bRadMult8Found = iNumRadMult8 > 0;
		bool bTricusFound = iNumTricuspidal > 0;

		while(!bStop && !bAbort){

			bool bSomeTried = false;
			
			for (int iIndex = 0; iIndex < iMaxTools && !bAbort; iIndex++)
			{
				if (WAIT_THREAD->IsStopped()) {
					bAbort = true;
					continue;
				}
				// Ad ogni giro il numero di colpi si massimizza
				iNumPunchRad4 = INT_MAX,
				iNumPunchRad8 = INT_MAX,
				iNumPunchCir = INT_MAX;
				iNumPunchTricus = INT_MAX;

				dRoughRadMult4Tool = DBL_MAX;
				dRoughRadMult8Tool = DBL_MAX;
				dRoughCirTool = DBL_MAX;
				dRoughTricuspTool = DBL_MAX;

				if (iNumRadMult4 > 0)
				{
					pToolRadMult4 = (CgToolRadMult4 *)aRadMult4Tools[iIndex];
					if(pToolRadMult4){

						bSomeTried = true;

						dRadRadMult4Tool = pToolRadMult4->GetNearestRadius(dRad);

						dEffRoughness = dRoughness;
						// Se l'utensile e` compatibile con le asperita` salvo l'area 
						// e il raggio per poi eventualmente confrontarlo con il migliore fra i circolari
						if (NibbleIsPossible(*pArc, pItemPrev, pItemNext, pToolRadMult4, dEffRoughness, dRoughnessEnds, aPoints, iRad4Index, dMinRoughness, true))
						{
							dRoughRadMult4Tool = dMinRoughness;

							// In base alle impostazioni dell'utente la scelta fra utensili viene 
							// fatta tra quelli pi� grandi o quelli che producono l'asperit� minore
							bool bPrefer = false;
							if(!bPreferToolByRough)
								bPrefer = true;
							else
								bPrefer = IsLess(dRoughRadMult4Tool, dRoughToProg);

							if (bPrefer) {
								// Verifico numero di colpi necessari
								c1 = aPoints[0];
								c2 = aPoints[1];
								CbVector vecStart = c1 - c;
								CbVector vecEnd = c2 - c;
								CmRic Ric(&GETPUNCHPROGRAM);
								Ric.SetRefPoint(c);
								Ric.SetUsedTool(pToolRadMult4);
								Ric.SetRadIndex(iRad4Index + 1);
								if (GetParameters().bProgRadRough)
									Ric.SetWidth(dRad+dMinRoughness);
								else
									Ric.SetWidth(dRad);
								Ric.SetNibAngle(vecEnd.Angle() - vecStart.Angle());
								Ric.SetSlope(vecStart.Angle());
								Ric.SetRoughness(dMinRoughness);
								// questa chiamata trova il numero di colpi
								int iNum = Ric.GET_NUM_OF_PUNCH;
								// Espansione funziona? Conviene rispetto a quelo che gia' ho?

								if (iNum > 0) {
									iNumPunchRad4 = iNum;
									dRadTool = pToolRadMult4->GetExtRadius();
									dMultRad4Radius = pToolRadMult4->GetRadius(iRad4Index);
									iBestToolNIB = MULT_RAD4;
									dRoughToProg = dMinRoughness;
									iIndexRad4Found = iIndex;
									if(!bPreferToolByRough){
										bStop = true;
									}
								}
							}
						}
					}
					// Decremento il numero di raggiatori multipli disponibili
					iNumRadMult4--;
				}

				if (iNumRadMult8 > 0)
				{
					pToolRadMult8 = (CgToolRadMult8 *)aRadMult8Tools[iIndex];
					if(pToolRadMult8){

						bSomeTried = true;

						dRadRadMult8Tool = pToolRadMult8->GetNearestRadius(dRad);

						dEffRoughness = dRoughness;
						// Se l'utensile e` compatibile con le asperita` salvo l'area 
						// e il raggio per poi eventualmente confrontarlo con il migliore fra i circolari
						if (NibbleIsPossible(*pArc, pItemPrev, pItemNext, pToolRadMult8, dEffRoughness, dRoughnessEnds, aPoints, iRad8Index, dMinRoughness, true))
						{
							dRoughRadMult8Tool = dMinRoughness;

							// In base alle impostazioni dell'utente la scelta fra utensili viene 
							// fatta tra quelli pi� grandi o quelli che producono l'asperit� minore
							bool bPrefer = false;
							if(!bPreferToolByRough)
								bPrefer = true;
							else
								bPrefer = IsLess(dRoughRadMult8Tool, dRoughToProg);

							if (bPrefer) {
								// Verifico numero di colpi necessari
								c1 = aPoints[0];
								c2 = aPoints[1];
								CbVector vecStart = c1 - c;
								CbVector vecEnd = c2 - c;
								CmRic Ric(&GETPUNCHPROGRAM);
								Ric.SetRefPoint(c);
								Ric.SetUsedTool(pToolRadMult8);
								Ric.SetRadIndex(iRad8Index + 1);
								if (GetParameters().bProgRadRough)
									Ric.SetWidth(dRad+dMinRoughness);
								else
									Ric.SetWidth(dRad);
								Ric.SetNibAngle(vecEnd.Angle() - vecStart.Angle());
								Ric.SetSlope(vecStart.Angle());
								Ric.SetRoughness(dMinRoughness);
								// questa chiamata trova il numero di colpi
								int iNum = Ric.GET_NUM_OF_PUNCH;
								// Espansione funziona? Conviene rispetto a quelo che gia' ho?

								if (iNum > 0 && iNum < iNumPunchRad4) {
									iNumPunchRad8 = iNum;
									dRadTool = pToolRadMult8->GetExtRadius();
									dMultRad8Radius = pToolRadMult8->GetRadius(iRad8Index);
									iBestToolNIB = MULT_RAD8;
									dRoughToProg = dMinRoughness;
									iIndexRad8Found = iIndex;
									if(!bPreferToolByRough){
										bStop = true;
									}
								}
							}
						}
					}
					// Decremento il numero di raggiatori multipli disponibili
					iNumRadMult8--;
				}


				if (iNumTricuspidal > 0)
				{
					pToolTricusp = (CgToolTricusp *)aTricuspTools[iIndex];

					if(pToolTricusp){

						bSomeTried = true;

						dRadTricuspTool = pToolTricusp->GetRadius();

						dEffRoughness = dRoughness;
						// Se l'utensile e` compatibile con le asperita` salvo l'area 
						// e il raggio per poi eventualmente confrontarlo con il migliore fra i circolari
						if (NibbleIsPossible(*pArc, pItemPrev, pItemNext, pToolTricusp, dEffRoughness, dRoughnessEnds, aPoints, iBuffer, dMinRoughness, true))
						{
							dRoughTricuspTool = dMinRoughness;

							// In base alle impostazioni dell'utente la scelta fra utensili viene 
							// fatta tra quelli pi� grandi o quelli che producono l'asperit� minore
							bool bPrefer = false;
							if(!bPreferToolByRough)
								bPrefer = true;
							else
								bPrefer = IsLess(dRoughTricuspTool, dRoughToProg);

							if (bPrefer) {
								// Verifico numero di colpi necessari
								c1 = aPoints[0];
								c2 = aPoints[1];
								CbVector vecStart = c1 - c;
								CbVector vecEnd = c2 - c;
								CmRic Ric(&GETPUNCHPROGRAM);
								Ric.SetRefPoint(c);
								Ric.SetUsedTool(pToolTricusp);
								if (GetParameters().bProgRadRough)
									Ric.SetWidth(dRad+dMinRoughness);
								else
									Ric.SetWidth(dRad);
								angStartTricus = vecStart.Angle();
								double dAng = angStartTricus.GetDeg();
								int iMul = (int)(dAng / 60.0);
								if (!IsSameValue(dAng, iMul * 60.0)) {
									if (IsSameValue(dAng, (iMul+1)*60.0)) {
										// Correggo leggere imprecisioni
										iMul++;
									} else {
										// Non e' multiplo di 60 gradi: devo trovare multiplo minore
										angStartTricus.SetDeg(iMul * 60.0);
									}
								}
								angNibTricus = vecEnd.Angle() - angStartTricus;
								dAng = angNibTricus.GetDeg();
								iMul = (int)(dAng / 60.0);
								if (!IsSameValue(dAng, iMul * 60.0)) {
									// Non e' multiplo di 60 gradi: devo trovare multiplo maggiore
									angNibTricus.SetDeg((iMul+1) * 60.0);
								}

								//-------------------------------------------------------------------
								// devo aggiornare c1 e c2 per i successivi controlli!!!!
								c1 = c + CbVector(vecStart.Len(), angStartTricus);
								c2 = c + CbVector(vecEnd.Len(), angStartTricus + angNibTricus);
								//--------------------------------------------------------------------

								Ric.SetNibAngle(angNibTricus);
								Ric.SetSlope(angStartTricus);
								Ric.SetRoughness(dMinRoughness);
								// questa chiamata trova il numero di colpi
								int iNum = Ric.GET_NUM_OF_PUNCH;
								// Espansione funziona? Conviene rispetto a quelo che gia' ho?

								if (iNum > 0 && iNum < iNumPunchRad4 && iNum < iNumPunchRad8) {
									iNumPunchTricus = iNum;
									dRadTool = dRadTricuspTool / 2.0;
									iBestToolNIB = TRICUSP;
									dRoughToProg = dMinRoughness;
									iIndexTricusFound = iIndex;
									if(!bPreferToolByRough){
										bStop = true;
									}
								}
							}
						}
					}
					// Decremento il numero di tricuspidi disponibili
					iNumTricuspidal--;
				}

				if (iNumCircular > 0)
				{
					pToolCirc = (CgToolCirc *)aCirTools[iIndex];
					if(pToolCirc){

						bSomeTried = true;
						
						dRadCirTool = pToolCirc->GetRadius();

						dEffRoughness = dRoughness;
						if (dEffRoughness > dRadCirTool*0.5)
							dEffRoughness = dRadCirTool*0.5;
						// Se c'e` compatibilita` di asperita`, lo confronto con gli eventuali 
						// utensili raggiatori multipli, e prendo il migliore
						if (NibbleIsPossible(*pArc, pItemPrev, pItemNext, pToolCirc, dEffRoughness, dRoughnessEnds, aPoints, iBuffer, dMinRoughness, true))
						{
							dRoughCirTool = dMinRoughness;
							
							// In base alle impostazioni dell'utente la scelta fra utensili viene 
							// fatta tra quelli pi� grandi o quelli che producono l'asperit� minore
							bool bPrefer = false;
							if(!bPreferToolByRough)
								bPrefer = true;
							else
								bPrefer = IsLess(dRoughCirTool, dRoughToProg);

							if (bPrefer)
							{
								c1 = aPoints[0];
								c2 = aPoints[1];
								CbVector vecStart = c1 - c;
								CbVector vecEnd = c2 - c;
								CmRic Ric(&GETPUNCHPROGRAM);
								Ric.SetRefPoint(c);
								Ric.SetUsedTool(pToolCirc);
								if (GetParameters().bProgRadRough)
									Ric.SetWidth(dRad+dMinRoughness);
								else
									Ric.SetWidth(dRad);
								Ric.SetNibAngle(vecEnd.Angle() - vecStart.Angle());
								Ric.SetSlope(vecStart.Angle());
								Ric.SetRoughness(dMinRoughness);
								// questa chiamata trova il numero di colpi
								int iNum = Ric.GET_NUM_OF_PUNCH;
								// Espansione funziona? Conviene rispetto a quelo che gia' ho?

								if (iNum > 0 && iNum < iNumPunchRad4 && iNum < iNumPunchRad8 && iNum < iNumPunchTricus) {
									iNumPunchCir = iNum;
									dRadTool = dRadCirTool;
									dRoughToProg = dMinRoughness;
									iBestToolNIB = CIRCULAR;
									iIndexCircFound = iIndex;
									if(!bPreferToolByRough){
										bStop = true;
									}
								}
							}
						}
					}
					// Decremento il numero di circolari disponibili
					iNumCircular--;
				}	
				if(bStop)
					break;
			}

			if (bAbort)
				continue;

			// se ho finito gli utensili da provare mi fermo
			if(!bSomeTried)
				bStop = true;

			// se sono al primo giro per la ricerca dell'utensile con
			// asperit� minore mi ricordo che al prossimo giro user�
			// l'algoritmo in base all'ingombro
			if(bPreferToolByRough)
				bPreferToolByRough = false;

			// popolo i dati per le routine seguenti che necessitano di sapere che
			// utensile � stato scelto
			if(bRadMult4Found)
				pToolRadMult4 = (CgToolRadMult4*)aRadMult4Tools[iIndexRad4Found];
			if(bRadMult8Found)
				pToolRadMult8 = (CgToolRadMult8*)aRadMult8Tools[iIndexRad8Found];
			if(bCircFound)
				pToolCirc = (CgToolCirc*)aCirTools[iIndexCircFound];
			if(bTricusFound)
				pToolTricusp = (CgToolTricusp*)aTricuspTools[iIndexTricusFound];


			// Se un utensile compatibile con le asperita` e` stato trovato, controllo se
			// la roditura interferisce con il bordo originale; se interferisce provo con un 
			// utensile piu` piccolo nel prossimo giro
			if (iBestToolNIB != NONE)
			{
				bool bNibOk = false;
				CbPolygon *pPolyNib = NULL;
				CgTool *pNibTool;
				int iRadIndex = 0;
				if(iBestToolNIB == MULT_RAD4)
				{
					pNibTool = pToolRadMult4;
					iRadIndex = iRad4Index;
				}
				else if(iBestToolNIB == MULT_RAD8)
				{
					pNibTool = pToolRadMult8;
					iRadIndex = iRad8Index;
				}
				else if (iBestToolNIB == TRICUSP)
					pNibTool = pToolTricusp;
				else
					pNibTool = pToolCirc;

				if (CUtilsGeom::CreatePolyNibbling(&GETPUNCHPROGRAM, pNibTool, iRadIndex, c, c1, c2, dRad, dEffRoughness, -CAMPUNCH_OFFSET_BORDER, 
											pPolyNib) && pPolyNib != NULL)
				{
					// Controllo se interagisce col bordo originale
					CgBorder *pOffBor = NULL;
					double dOff = 2.0*resabs;
					while (pOffBor == NULL && dOff > resabs*0.01) {
						dOff = dOff/2.0;
						pOffBor = borderOrig.Offset(dOff);
					}
					if (pOffBor == NULL)
						pOffBor = new CgBorder(borderOrig);
					CbPolygon *pPolyBorder = pOffBor->GetPolygon(resabs, false);
					bNibOk = CUtilsGeom::IsNibblingContained(*pPolyNib, *pPolyBorder, 0.005);
					delete pPolyNib; pPolyNib = NULL;
					delete pPolyBorder;
					pOffBor->DeleteAllItems();
					delete pOffBor;
				}
				if (!bNibOk){
					// questi utensili non vanno bene: al prossimo giro gli ignoro
					iNumCircular = iNumCircFound;
					iNumRadMult4 = iNumRadMult4Found;
					iNumRadMult8 = iNumRadMult8Found;
					iNumTricuspidal = iNumTricusFound;
					if(iBestToolNIB == MULT_RAD4)
						aRadMult4Tools[iIndexRad4Found] = NULL;
					else if(iBestToolNIB == MULT_RAD8)
						aRadMult8Tools[iIndexRad8Found] = NULL;
					else if (iBestToolNIB == TRICUSP)
						aTricuspTools[iIndexTricusFound] = NULL;
					else
						aCirTools[iIndexCircFound] = NULL;
					iBestToolNIB = NONE;
					bStop = false;
				}
				else
				{
					bStop = true;
				}
			}
		}
	}

	// Se e` stato trovato il migliore utensile compatibile con le asperita` e che
	// permette una roditura che non interferisce col bordo originale, programmo
	// la roditura e vado avanti

	if (!bAbort && iBestToolNIB != NONE)
	{
		CbVector vecStart = c1 - c;
		CbVector vecEnd = c2 - c;

		// Programmazione della punzonatura e montaggio dell'utensile
		CmRic *pRic = new CmRic(&GETPUNCHPROGRAM);
		pRic->SetRefPoint(c);
		pRic->SetSlope(vecStart.Angle());
		pRic->SetNibAngle(vecEnd.Angle() - vecStart.Angle());
		switch (iBestToolNIB)
		{
			case MULT_RAD4:
				pRic->SetUsedTool(pToolRadMult4);
				pRic->SetRadIndex(iRad4Index);
				dMultRad4Radius = pToolRadMult4->GetRadius(iRad4Index);
				break;
			case MULT_RAD8:
				pRic->SetUsedTool(pToolRadMult8);
				pRic->SetRadIndex(iRad8Index);
				dMultRad8Radius = pToolRadMult8->GetRadius(iRad8Index);
				break;
			case TRICUSP:
				pRic->SetUsedTool(pToolTricusp);
//					pRic->SetSlope(angStartTricus);
//					pRic->SetNibAngle(angNibTricus);
				break;
			case CIRCULAR:
				pRic->SetUsedTool(pToolCirc);
		}
#if 0
		if (GetParameters().bProgRadRough)
			pRic->SetWidth(dRad+dEffRoughness);
		else
			pRic->SetWidth(dRad);
#else
		// Se uso 4/8 raggi con raggio identico non programmo raggio+asperita'
		if (!(GetParameters().bProgRadRough) ||
			(iBestToolNIB == MULT_RAD4 && IsSameValue(dMultRad4Radius, dRad)) ||
			(iBestToolNIB == MULT_RAD8 && IsSameValue(dMultRad8Radius, dRad))) 
			pRic->SetWidth(dRad);
		else
			pRic->SetWidth(dRad+dEffRoughness);
#endif

		pRic->SetRoughness(dEffRoughness);

		lWIArc.AddTail(pRic);

		// Ampiezza della corona circolare coperta dalla prima roditura: dipende
		// dall'utensile utilizzato
		double dThickNib;
		double dWidthCut = GetConstants().dWidthCut;
		if (iBestToolNIB == CIRCULAR)
			dThickNib = 2.0 * dRadTool - dRoughCirTool - dWidthCut;
		else if (iBestToolNIB == TRICUSP)
			dThickNib = ThickTricuspNib(dRad, pToolTricusp, dRoughTricuspTool);
		else if (iBestToolNIB == MULT_RAD4)
			dThickNib = ThickMultRadNib(dRad, pToolRadMult4, iRad4Index, dRoughRadMult4Tool) - dWidthCut;
		else if (iBestToolNIB == MULT_RAD8)
			dThickNib = ThickMultRadNib(dRad, pToolRadMult8, iRad8Index, dRoughRadMult8Tool) - dWidthCut;


		// Controllo se con la prima roditura e` gia` tutto risolto
		if (IsGreaterEqual(dThickNib, dMinDist)) 
			return 1;

		// Raggio dell'arco ottenuto riducendo l'arco originale della ampiezza
		// della corona coperta dalla prima NIB. 
		double dRadRed = dRad - dThickNib;

		// Se dMinDist e` maggiore del raggio, significa che la spezzata di
		// sostituzione entra poco nell'arco. Se e` cosi` oppure se l'arco ha 
		// un'ampiezza superiore a 200 gradi (valore empirico),
		// innanzitutto controllo se e` possibile risolvere il tutto con un singolo
		// colpo (questo stesso controllo viene fatto anche nella routine ricorsiva
		// che fa il resto della sgrossatura se la gestione particolare degli archi
		// con angolo > 200 non porta a una soluzione definitiva dell'arco); se non 
		// e` possibile costruisco un cerchio avente lo stesso centro 
		// dell'arco e raggio pari a quello dell'arco ridotto incrementato
		// dell'asperita` richiesta e controllo se tale cerchio interferisce col 
		// resto del bordo. Se non interferisce, chiamo direttamente la routine che 
		// distrugge il cerchio
		bool bSolved = false;
		if (IsGreater(dMinDist, dRad) || IsGreater(pArc->GetAngle().GetDeg(), 200.0))
		{
			
			// calcolo il raggio acora da rodere nel seguente modo:
			// - se l'angolo che resta da fare rispetto a 2PI � minore del minimo angolo
			//		necessario perch� le punzonature estreme di una RIC fatta con l'utensile
			//		usato precedentemente si tocchino; allora resta da fare il raggio meno il
			//		raggio dell'utensile. Perch� la RIC si chiude agli estremi e la parte di
			//		arco che resta perch� si chiuda in un cerchio � gi� coperta. Deve per�
			//		anche essere verificato che l'asperit� che creo non chiudendo completamente
			//		l'angolo 2PI non sia superiore all'asperit� finale impostata da parametri
			// - altrimenti resta da fare il massimo tra il raggio ridotto dalla RIC e la
			//		differenza tra dMinDist e il raggio dell'arco
			// Nel caso dell'utensile multiraggiato per il calcolo dell'asperit� devo usare
			// il raggio effettivamante usato dell'utensile
			CbAngle aNIB(2.0 * M_PI - (vecEnd.Angle() - vecStart.Angle()));
			double dRoughRadius = dRadTool;
			if(iBestToolNIB == MULT_RAD4)
				dRoughRadius = dMultRad4Radius;
			else if(iBestToolNIB == MULT_RAD8)
				dRoughRadius = dMultRad8Radius;
			double dRealDist = (dRad - dRoughRadius) * sin(aNIB.Get());
			double dRoughToClose = CUtilsTools::MinRoughCir(dRad, dRoughRadius, dRealDist, true);
			double dRadSingleCir;
			CbAngle aToClose(2.0 * atan2(dRoughRadius, dRad - dRoughRadius));
			if((vecEnd.Angle() - vecStart.Angle()) > 1.5 * M_PI && 
				aToClose > (2.0 * M_PI - (vecEnd.Angle() - vecStart.Angle())) &&
				dRoughToClose < dRoughnessEnds)
				dRadSingleCir = dRad - dRadTool;
			else
				dRadSingleCir = mymax(dRadRed, dMinDist - dRad);
			
			// Controllo se posso risolvere con un singolo colpo
			iNumOfTools = GET_TOOL_DB.FindCirc(aCirTools, iToolType, CbCond(dRadSingleCir, 0, dRad - dRadSingleCir), true);
			if (iNumOfTools > 0)
			{
				pToolCirc = (CgToolCirc *)aCirTools[0];

				// Controllo se il tool interferisce con il bordo originale
				CgCircle cirTool (c, pToolCirc->GetRadius());
				CgBorder borTool;
				borTool.AddItem(&cirTool);
				int nNumInt = Intersect(borTool, borderOrig, aPoints);
				borTool.Reset();
				if (nNumInt == 0)
				{
					// Programmazione della punzonatura
					CmXyp *pXyp = new CmXyp(&GETPUNCHPROGRAM);
					pXyp->SetRefPoint(c);
					pXyp->SetUsedTool(pToolCirc);
					lWIArc.AddTail(pXyp);
					return 1;
				}
			}

			// Non ho risolto con un singolo colpo: provo con la roditura
			// utilizzando la routine dei bordi circolari
			CgCircle cirReduced (c, dRadSingleCir + dEffRoughness);

			CAMBORDER borCirc;
			borCirc.AddItem(&cirReduced);
			int nNumInt = Intersect(borCirc, borderOrig, aPoints);
			if (nNumInt == 0) {
				// Se riesco a punzonare il bordo circolare fittizio creato,
				// devo allineare le strutture, nel senso che la macro associata
				// (e` una perche` basta comunque una macro per punzonare un cerchio)
				// al bordo fittizio deve far parte dell'array di macro
				// associate al bordo che sto considerando

				PUNCH_INSTRUCTION_LIST lWITmp;
				if (PunchCircle(borCirc, iToolType, bUseNibbling, false, lWITmp))
				{
					bSolved = true;
					// append lWItmp to lWIArc
					lWIArc.AddTail(&lWITmp);
				} 
				else 
				{
					// cancello istruzioni
					lWITmp.DeleteAll();
				}
			}
			borCirc.Reset();
		}

		if (WAIT_THREAD->IsStopped()) 
			bAbort = true;

		bool bTotallyResolved = true;
		if (!bSolved && !bAbort) {
			// Controllo quante intersezioni ha la prima roditura con la spezzata di
			// sostituzione. Per questo considero l'arco che ha raggio pari a dRadRed
			// e ampiezza identificata dai centri iniziale e finale della roditura.
			// Il numero di intersezioni di questo arco con la spezzata di sostituzione
			// deve essere pari: se ha due (dovrebbero essere almeno due!) intersezioni, 
			// rimane un unico arco ridotto da rodere; se ha quattro intersezioni, gli 
			// archi rimanenti da rodere sono due, e cosi` via.
			// Individuato il numero di archi, si invoca una procedura ricorsiva
			// per ogni arco (ricorsiva perche` ogni arco puo` a sua volta portare
			// a considerare piu` archi)
			CgArc arcMinNib (c, dRadRed, pArc->GetStartVect(), CbSignedAngle(pArc->GetAngle().Get()));
			CgBorder borArc;
			borArc.AddItem(&arcMinNib);

			if (0) {
				border.SetPen(2);
				borArc.SetPen(9);
				border.Show();
				borArc.Show();
				pISM->WaitForUser();
			}
			int iInters = Intersect(borArc, border, aPoints);
			borArc.Reset();
			// Il numero di intersezioni deve essere pari oppure uno (in tal caso la roditura
			// ha completato proprio a pelo la distanza minima)
			// ASSERT_EXC(iInters == 1 || IsSameValue((double) iInters / 2.0, floor((double) iInters / 2.0)));

			if (iInters <= 1) {
				// Provo a calcolare intersezioni con cerchio ridotto e tengo quelle vicine ai limiti
				// oppure interne all'arco ridotto. Caso in cui il bordo ridotto non interseca e' venuto fuori
				// considerando un arco di raggio grande e arco leggermente superiore ai 180 gradi, la cui spezzata
				// di sostituzione e' la linea che unisce gli estremi dell'arco. In questo caso l'arco ridotto
				// ha angolo sotteso identico a arco originale ma i suoi estremi non toccano la linea di sostituzione
				// proprio perche' superiore a 180 gradi.
				CgCircle cir(c, dRadRed);
				borArc.AddItem(&cir);
				if (0) {
					border.SetPen(2);
					borArc.SetPen(9);
					border.Show();
					borArc.Show();
					pISM->WaitForUser();
				}
				aPoints.RemoveAll();
				iInters = Intersect(borArc, border, aPoints);
				if (iInters > 2) {
					CbPoint ptStartNewArc = arcMinNib.GetStart();
					CbPoint ptEndNewArc = arcMinNib.GetEnd();
					// Devo tenere solo quelli interessanti
					CbPointArray aNewPoints;
					// Cerco i piu' vicini agli estremi dell'arco ridotto
					for (int iPoint = 0; iPoint<2; iPoint++) {
						CbPoint ptEstremo;
						if (iPoint == 0) {
							ptEstremo = ptStartNewArc;
						} else {
							ptEstremo = ptEndNewArc;
						}
						double dCurrMin = Distance(ptEstremo, aPoints[0]);
						CbPoint ptNearest = aPoints[0];
						for (int iInts = 1; iInts < iInters; iInts++) {
							double dCurr = Distance(ptEstremo, aPoints[iInts]);
							if (dCurr < dCurrMin) {
								ptNearest = aPoints[iInts];
								dCurrMin = dCurr;
							}
						}
						aNewPoints.Add(ptNearest);
						if (iPoint == 0) {
							ptStartNewArc = ptNearest;
						} else {
							ptEndNewArc = ptNearest;
						}
					}
					// Poi prendo eventuali interni all'arco ridotto
					for (int iInts = 1; iInts < iInters; iInts++) {
						CbPoint ptt = aPoints[iInts];
						if (arcMinNib.Includes(ptt))
							aNewPoints.Add(ptt);
					}
					aPoints.RemoveAll();
					aPoints.Append(aNewPoints);
					iInters = aPoints.GetSize();
					arcMinNib.SetStart(ptStartNewArc);
					arcMinNib.SetEnd(ptEndNewArc);
				}
				borArc.Reset();
			}
			if (iInters > 1)
			{
				// Ordinamento angolare dei punti lungo l'arco inferiore del profilo
				// di roditura									 
				CUtilsGeneral::SortByAngle(aPoints, c, arcMinNib.GetStart());

				// A questo punto chiamo una procedura ricorsiva che distrugge
				// il resto dell'arco. Creo prima una variabile che sara` la copia del valore del raggio
				// minimo, in quanto, essendo passato per valore, se venisse passato direttamente, verrebbe
				// modificato ad ogni ciclo del for e quindi non va bene
				double dRadRedBuff;
				bool bOnlySingleHit = false;
				for (int i=0; i>0, i<iInters; i+=2)
				{
					if (i+1 >= iInters) {
						// Numero di intersezioni dispari: segnalo come non eseguita la parte 
						// corrispondente all'ultima intersezione
						if (bOnlySingleHit) {
							border.SetCurrItemPos(posLastSub);
							for (int k=0; k<borderSubs.CountItems() - 1; k++)
								border.GetPrevItem();
							border.ReplaceItems(borderSubs.CountItems(), &borderSubs, true);
							bCompleteCircleIsPossible = false;
						} else {
							// Poiche` la parte interna dell'arco non e` stata completamente distrutta,
							// devo creare un bordo che corrisponde all'area rimasta non risolta. La
							// variabile dRadRedBuff e` passata per valore e quindi a questo punto
							// tale variabile identifica la profondita` della parte non distrutta. L'area
							// non risolta la creo quindi come area compresa fra la spezzata di sostituzione
							// e l'arco concentrico con l'arco da rodere e avente raggio pari a dRadRedBuff.
							// Un un giro di NIB si possono formare piu` aree disgiunte non risolte, quindi
							// uso un border array
							CbPoint pointUnres;
							CgBaseBorderArray aborUnresolved;
							CgArc arcNotDone(aPoints[i],dThickNib,X_AXIS, 90.0);
							CreateUnresolvedBorder(arcNotDone, borderSubs, dThickNib, aborUnresolved, pointUnres);

							// aree non risolte
							lWIArc.AddTail(new CmUra(&GETPUNCHPROGRAM, pointUnres));
							bTotallyResolved = false;
						}
						continue;
					}
					CbVector v1 (aPoints[i] - c);
					CbVector v2 (aPoints[i+1] - c);
					dRadRedBuff = dRadRed;
					if (iInters < 3)
						bOnlySingleHit = true;
					if (!DoInternalNib(c, 
									   dRad, 
									   dRadRedBuff, 
									   CbUnitVector(v1.Angle()), 
									   CbUnitVector(v2.Angle()), 
									   border, 
									   borderOrig,
									   pItemPrev,
									   pItemNext,
									   dMinDist - dThickNib, 
									   iToolType,
									   bOnlySingleHit,
									   lWIArc))
					{
						if (bOnlySingleHit) {
							// Ho richiesto solo la verifica per eseguire l'arco rimasto 
							// con colpo singolo. Essa e' andata male: cambio spezzata
							// di sostituzione in maniera da finire la distruzione
							// dello sfrido interno con dei rettangoli.
							int iNumItemsSubs = borderSubs.CountItems();
							CbSignedAngle ang ((v2.Angle() - v1.Angle()).Get());
							CgArc arcNew(c, dRadRed, v1, ang);
							if (!ModPolylineForRIC(*pArc, arcNew, dRoughToProg, borderSubs)) {
								CbPoint pointUnres;
								CgBaseBorderArray aborUnresolved;
								CreateUnresolvedBorder(*pArc, borderSubs, dRadRedBuff, aborUnresolved, pointUnres);

								// aree non risolte
								lWIArc.AddTail(new CmUra(&GETPUNCHPROGRAM, pointUnres));
								bTotallyResolved = false;
							} else {
								// Vado all'inizio della vecchia spezzata
								border.SetCurrItemPos(posLastSub);
								for (int k=0; k<iNumItemsSubs - 1; k++)
									border.GetPrevItem();
								border.ReplaceItems(iNumItemsSubs, &borderSubs, true);
								bCompleteCircleIsPossible = false;
							}
						} else {
							// Poiche` la parte interna dell'arco non e` stata completamente distrutta,
							// devo creare un bordo che corrisponde all'area rimasta non risolta. La
							// variabile dRadRedBuff e` passata per valore e quindi a questo punto
							// tale variabile identifica la profondita` della parte non distrutta. L'area
							// non risolta la creo quindi come area compresa fra la spezzata di sostituzione
							// e l'arco concentrico con l'arco da rodere e avente raggio pari a dRadRedBuff.
							// Un un giro di NIB si possono formare piu` aree disgiunte non risolte, quindi
							// uso un border array
							CbPoint pointUnres;
							CgBaseBorderArray aborUnresolved;
							CreateUnresolvedBorder(*pArc, borderSubs, dRadRedBuff, aborUnresolved, pointUnres);

							// aree non risolte
							lWIArc.AddTail(new CmUra(&GETPUNCHPROGRAM, pointUnres));
							bTotallyResolved = false;
						}
					}
				}
			}
		}
		if (bAbort) {
			return -1;
		} else if (!bCompleteCircleIsPossible) {
			if (bTotallyResolved)
				return 1;
			else
				return 0;
		} else {
			// Verifico se conviene programmare la distruzione dell'intero cerchio
			// piuttosto che la roditura dell'arco in piu' passate (in effetti
			// ho gia' accettao una roditura composta da una passata esterna piu'
			// un colpo singolo centrale)

			bool bRodituraIsBest = true;
			// Definisco cerchio completo
			CgCircle cir (c, dRad);
			CAMBORDER borCirc;
			borCirc.AddItem(&cir);
			PUNCH_INSTRUCTION_LIST lWICircle;
			if (PunchCircle(borCirc, iToolType, bUseNibbling, false, lWICircle)) {

				// Devo verificare quale programmazione conviene tenere
				if (!bTotallyResolved) {
					// Roditura non era riuscita a completare l'arco: meglio 
					// distruggere tutto il cerchio
					bRodituraIsBest = false;
				} else {
					// Devo contare il numero di colpi
					int nHitRoditura = 0;
					POSITION posLastOld = lWIArc.FindIndex(iOldStmtNum);
					POSITION pos;
					for(pos = posLastOld; pos != NULL; )
					{
						PUNCH_INSTRUCTION* pWI = lWIArc.GetNext(pos);
						if (pWI != NULL)
							nHitRoditura += pWI->GET_NUM_OF_PUNCH;
					}

					int nHitCircle = 0;
					for(pos = lWICircle.GetHeadPosition(); pos != NULL; )
					{
						PUNCH_INSTRUCTION* pWI = lWICircle.GetNext(pos);
						if (pWI != NULL)
							nHitCircle += pWI->GET_NUM_OF_PUNCH;
					}
					
					if (nHitCircle > 0 && nHitCircle <= nHitRoditura)
						bRodituraIsBest = false;
				}
			} 
			
			if (bRodituraIsBest) {
				// Cancello istruzioni eventualmente aggiunte da PunchCircle
				lWICircle.DeleteAll();
				if (bTotallyResolved)
					return 1;
				else
					return 0;
			} else {
				// cancello istruzioni di roditura dell'arco e lascio quelle create da PunchCircle
				while (lWIArc.GetCount() > iOldStmtNum)
				{
					PUNCH_INSTRUCTION* pWI = lWIArc.RemoveTail();
					if (pWI != NULL)
						delete pWI;
				}
									
				// Copio istruzioni nell'array di output
				lWIArc.AddTail(&lWICircle);
				return 1;
			}
		}
	}
	else if (bAbort) 
	{
		return -1;
	}
	else
	{
		// aree non risolte
		lWIArc.AddTail(new CmUra(&GETPUNCHPROGRAM, ptCenterToPunch));
		return 0;
	}

	return (bAbort ? -1 : 0);	
}

//----------------------------------------------------------------------
//
// Description: Controlla se e` possibile impostare la POL per la roditura
//              di un arco, in modo che non ci siano interferenze col
//              bordo della tranciatura
//
// Return values: true se possibile, false altrimenti
//                angPitch - passo angolare effettivo della POL
//                iNumOfPunches - numero di punzonature
// Arguments:
//	 arc -  arco da rodere
//   borTool - bordo associato all'utensile rettangolare
//   dHeightTool - altezza dell'utensile
//   borOrig - bordo della tranciatura
//   ang1 - angolo iniziale
//   ang2 - angolo finale
//   angPitchMax - passo angolare massimo della POL
//
//----------------------------------------------------------------------
bool 
CArcsBorderWorkStrategy::PolIsPossible(CgArc &arc, CgBorder &borTool, double dHeightTool, CgBorder &borOrig, CbAngle &ang1, CbAngle &ang2, CbAngle &angPitchMax, CbAngle &angPitch, int &iNumOfPunches)
{
	CbPoint c = arc.GetCenter();
	CbPoint	centerTool;
	CbPointArray apt;

	// Distanza fra il centro dell'utensile e il centro dell'arco da rodere
	double dDist = arc.GetRadius() + dHeightTool / 2.0 + CAMPUNCH_OFFSET_BORDER;

	// Angolo da coprire
	CbAngle ang = ang2 - ang1;

	// Numero di colpi necessari
	iNumOfPunches = (int) ceil(ang.Get() / angPitchMax.Get()) + 1;
	// Passo angolare effettivo
	angPitch = CbAngle(ang.Get() / (double)(iNumOfPunches - 1));

	// Do` all'utensile la rotazione iniziale
	borTool.ZRotate(borTool.GetCenter(), CbSignedAngle((ang1 + M_PI_2).Get()));
	centerTool = c + CbUnitVector(ang1) * dDist;
	borTool.Move(centerTool - borTool.GetCenter());

	for (int i=1; i<iNumOfPunches; i++)
	{
		// Posiziono l'utensile
		CbUnitVector unitV (ang1 + angPitch * i);
		borTool.ZRotate(borTool.GetCenter(), CbSignedAngle(angPitch.Get()));
		centerTool = c + unitV * dDist;
		borTool.Move(centerTool - borTool.GetCenter());

		// Verifico se interferisce col resto del bordo
		if (Intersect(borTool, borOrig, apt) != 0)
			return false;
	}

	return true;
}

//----------------------------------------------------------------------
//
// Description: Modifica la spezzata di sostituzione dell'arco in 
//              conseguenza della programmazione dell'istruzione POL
//
// Arguments:
//	 arc -  arco da rodere
//   borSubs - bordo costituito dalla spezzata da modificare
//   dDelta - differenza fra il raggio dell'arco e il raggio della
//            circonferenza concentrica che serve per individuare
//            di quanto si estende la spezzata all'esterno dell'arco
// 
// Note: - l'arco e` supposto orario (la POL viene programmata solo
//         per archi orari)
//		 - ciascun tratto della spezzata viene disegnato solo se la sua
//         lunghezza e` superiore alla tolleranza resabs
//
//----------------------------------------------------------------------
void 
CArcsBorderWorkStrategy::ModPolylineForPOL(CgArc &arc, CgBorder &borSubs, double dDelta)
{

	int				i;
	CbPoint			c = arc.GetCenter(),
					p1, p2, p;
	double			dRad = arc.GetRadius();
	double			dRadExt = dRad + dDelta;
	CgLine			line;
	CbPointArray	apt;
	int				iNumInters;
	bool			bLastSeg;

	double resabsSav = resabs;
	resabs /= 10.0;

	// Cerchio concentrico con l'arco e avente raggio pari a quello
	// dell'arco piu` dDelta
	CgCircle circle (c, dRadExt);

	// Numero di quadranti attraversati dall'arco
	int iQuadrants = arc.CrossingQuadrants();

	// Quadrante dello start point dell'arco (il primo quadrante ha indice 0)
	int iQuadStart = (int) floor((arc.GetStartAngle().Get() - resnor) / M_PI_2);
	// Se il quadrante dello start point e` risulta -1, allora l'angolo di tale
	// punto e` zero; tenendo conto che l'arco e` orario, il quandrante sara`
	// quindi il quarto
	if (iQuadStart < 0)
		iQuadStart = 3;

	p1 = arc.GetStart();
	// Quadrante corrente
	int iCurrQuad = iQuadStart;

	// Tolleranza per essere conservativi
	double dReduction = GetConstants().dWidthCut;

	// Svuoto il bordo della spezzata
	borSubs.Reset();

	double dDist;
	CgLine *pLine;
	for (i=0; i<iQuadrants; i++)
	{
		bLastSeg = false;
		switch (iCurrQuad)
		{
			case 0:
				if (i == iQuadrants -1)
					p2 = arc.GetEnd();
				else
					p2 = c + CbUnitVector(0.0) * dRad;
				dDist = p2.x() - p1.x();
				while (IsGreater(dDist, 0.0))
				{
					// Tratto orizzontale

					line.SetStart(p1);
					// Linea lunga abbastanza da intersecare la circonferenza esterna
					line.SetEnd(p1 + CbUnitVector(0.0) * dRadExt);
					iNumInters = Intersect(line, circle, apt, true);
					ASSERT_EXC(iNumInters == 1);
					if (dDist - Distance(p1, apt[0]) + dReduction < 0)
					{
						p = p1 + CbUnitVector(0.0) * dDist;
						bLastSeg = true;
					}
					else
						p = p1 + CbUnitVector(0.0) * (Distance(p1, apt[0]) - dReduction);
					if (Distance(p1, p) > resabs)
					{
						pLine = new CgLine(p1, p);
						borSubs.SetAutoCheck(false);
						borSubs.AddItem(pLine);
						borSubs.SetAutoCheck(true);
						dDist = dDist - Distance(p1, apt[0]) + dReduction;
					}

					// Tratto verticale

					if (bLastSeg)
					{
						if (Distance(p, p2) > resabs)
						{
							pLine = new CgLine(p, p2);
							borSubs.SetAutoCheck(false);
							borSubs.AddItem(pLine);
							borSubs.SetAutoCheck(true);
						}
					}
					else
					{
						p1 = p;
						line.SetStart(p1);
						// L'end point lo imposto in modo che la linea abbia sicuramente
						// una sola intersezione con l'arco
						line.SetEnd(p1 + CbUnitVector(3.0 * M_PI_2) * (p1.y() - c.y()));
						iNumInters = Intersect(line, arc, apt, true);
						ASSERT_EXC(iNumInters == 1);
						p = p1 + CbUnitVector(3.0 * M_PI_2) * (Distance(p1, apt[0]) - dReduction);
						if (Distance(p1, p) > resabs)
						{
							pLine = new CgLine(p1, p);
							borSubs.SetAutoCheck(false);
							borSubs.AddItem(pLine);
							borSubs.SetAutoCheck(true);
						}
					}

					p1 = p;
				}

				iCurrQuad = 3;
				break;

			case 1:
				if (i == iQuadrants -1)
					p2 = arc.GetEnd();
				else
					p2 = c + CbUnitVector(M_PI_2) * dRad;

				dDist = p2.y() - p1.y();
				while (IsGreater(dDist, 0.0))
				{
					// Tratto verticale

					line.SetStart(p1);
					// Linea lunga abbastanza da intersecare la circonferenza esterna
					line.SetEnd(p1 + CbUnitVector(M_PI_2) * dRadExt);
					iNumInters = Intersect(line, circle, apt, true);
					ASSERT_EXC(iNumInters == 1);
					if (dDist - Distance(p1, apt[0]) + dReduction < 0)
					{
						p = p1 + CbUnitVector(M_PI_2) * dDist;
						bLastSeg = true;
					}
					else
						p = p1 + CbUnitVector(M_PI_2) * (Distance(p1, apt[0]) - dReduction);
					if (Distance(p1, p) > resabs)
					{
						CgLine *pLine = new CgLine(p1, p);
						borSubs.SetAutoCheck(false);
						borSubs.AddItem(pLine);
						borSubs.SetAutoCheck(true);
						dDist = dDist - Distance(p1, apt[0]) + dReduction;
					}

					// Tratto orizzontale

					if (bLastSeg)
					{
						if (Distance(p, p2) > resabs)
						{
							pLine = new CgLine(p, p2);
							borSubs.SetAutoCheck(false);
							borSubs.AddItem(pLine);
							borSubs.SetAutoCheck(true);
						}
					}
					else
					{
						p1 = p;
						line.SetStart(p1);
						// L'end point lo imposto in modo che la linea abbia sicuramente
						// una sola intersezione con l'arco
						line.SetEnd(p1 + CbUnitVector(0.0) * (c.x() - p1.x()));
						iNumInters = Intersect(line, arc, apt, true);
						ASSERT_EXC(iNumInters == 1);
						p = p1 + CbUnitVector(0.0) * (Distance(p1, apt[0]) - dReduction);
						if (Distance(p1, p) > resabs)
						{
							pLine = new CgLine(p1, p);
							borSubs.SetAutoCheck(false);
							borSubs.AddItem(pLine);
							borSubs.SetAutoCheck(true);
						}
					}

					p1 = p;
				}

				iCurrQuad = 0;
				break;

			case 2:
				if (i == iQuadrants -1)
					p2 = arc.GetEnd();
				else
					p2 = c + CbUnitVector(M_PI) * dRad;

				dDist = p1.x() - p2.x();
				while (IsGreater(dDist, 0.0))
				{
					// Tratto orizzontale

					line.SetStart(p1);
					// Linea lunga abbastanza da intersecare la circonferenza esterna
					line.SetEnd(p1 + CbUnitVector(M_PI) * dRadExt);
					iNumInters = Intersect(line, circle, apt, true);
					ASSERT_EXC(iNumInters == 1);
					if (dDist - Distance(p1, apt[0]) + dReduction < 0)
					{
						p = p1 + CbUnitVector(M_PI) * dDist;
						bLastSeg = true;
					}
					else
						p = p1 + CbUnitVector(M_PI) * (Distance(p1, apt[0]) - dReduction);
					if (Distance(p1, p) > resabs)
					{
						CgLine *pLine = new CgLine(p1, p);
						borSubs.SetAutoCheck(false);
						borSubs.AddItem(pLine);
						borSubs.SetAutoCheck(true);
						dDist = dDist - Distance(p1, apt[0]) + dReduction;
					}

					// Tratto verticale

					if (bLastSeg)
					{
						if (Distance(p, p2) > resabs)
						{
							pLine = new CgLine(p, p2);
							borSubs.SetAutoCheck(false);
							borSubs.AddItem(pLine);
							borSubs.SetAutoCheck(true);
						}
					}
					else
					{
						p1 = p;
						line.SetStart(p1);
						// L'end point lo imposto in modo che la linea abbia sicuramente
						// una sola intersezione con l'arco
						line.SetEnd(p1 + CbUnitVector(M_PI_2) * (c.y() - p1.y()));
						iNumInters = Intersect(line, arc, apt, true);
						ASSERT_EXC(iNumInters == 1);
						p = p1 + CbUnitVector(M_PI_2) * (Distance(p1, apt[0]) - dReduction);
						if (Distance(p1, p) > resabs)
						{
							pLine = new CgLine(p1, p);
							borSubs.SetAutoCheck(false);
							borSubs.AddItem(pLine);
							borSubs.SetAutoCheck(true);
						}
					}

					p1 = p;
				}

				iCurrQuad = 1;
				break;

			case 3:
				if (i == iQuadrants -1)
					p2 = arc.GetEnd();
				else
					p2 = c + CbUnitVector(3.0 * M_PI_2) * dRad;

				dDist = p1.y() - p2.y();
				while (IsGreater(dDist, 0.0))
				{
					// Tratto verticale

					line.SetStart(p1);
					// Linea lunga abbastanza da intersecare la circonferenza esterna
					line.SetEnd(p1 + CbUnitVector(3.0 * M_PI_2) * dRadExt);
					iNumInters = Intersect(line, circle, apt, true);
					ASSERT_EXC(iNumInters == 1);
					if (dDist - Distance(p1, apt[0]) + dReduction < 0)
					{
						p = p1 + CbUnitVector(3.0 * M_PI_2) * dDist;
						bLastSeg = true;
					}
					else
						p = p1 + CbUnitVector(3.0 * M_PI_2) * (Distance(p1, apt[0]) - dReduction);
					if (Distance(p1, p) > resabs)
					{
						CgLine *pLine = new CgLine(p1, p);
						borSubs.SetAutoCheck(false);
						borSubs.AddItem(pLine);
						borSubs.SetAutoCheck(true);
						dDist = dDist - Distance(p1, apt[0]) + dReduction;
					}

					// Tratto orizzontale

					if (bLastSeg)
					{
						if (Distance(p, p2) > resabs)
						{
							pLine = new CgLine(p, p2);
							borSubs.SetAutoCheck(false);
							borSubs.AddItem(pLine);
							borSubs.SetAutoCheck(true);
						}
					}
					else
					{
						p1 = p;
						line.SetStart(p1);
						// L'end point lo imposto in modo che la linea abbia sicuramente
						// una sola intersezione con l'arco
						line.SetEnd(p1 + CbUnitVector(M_PI) * (p1.x() - c.x()));
						iNumInters = Intersect(line, arc, apt, true);
						ASSERT_EXC(iNumInters == 1);
						p = p1 + CbUnitVector(M_PI) * (Distance(p1, apt[0]) - dReduction);
						if (Distance(p1, p) > resabs)
						{
							pLine = new CgLine(p1, p);
							borSubs.SetAutoCheck(false);
							borSubs.AddItem(pLine);
							borSubs.SetAutoCheck(true);
						}
					}

					p1 = p;
				}

				iCurrQuad = 2;
		}

		p1 = p2;
	}

	// A questo punto la nuova spezzata di sostituzione e` completa
	resabs = resabsSav;

}

//----------------------------------------------------------------------
//
// Description: Modifica la spezzata di sostituzione dell'arco in 
//              conseguenza della programmazione di un'istruzione RIC
//
// Arguments:
//	 arcOrig -  arco da rodere
//	 arcNew - nuovo arco di raggio ridotto che sarebbe da rodere dopo 
//			  la roditura gia' programmata
//	 dAsper - asperita' programmata nella roditura dell'arco
//   borSubs - bordo costituito dalla spezzata da modificare
// 
// Note: - l'arco e` supposto antiorario (la RIC viene programmata solo
//         per tali archi)
//		 - ciascun tratto della spezzata viene disegnato solo se la sua
//         lunghezza e` superiore alla tolleranza resabs
//
//----------------------------------------------------------------------
bool 
CArcsBorderWorkStrategy::ModPolylineForRIC(CgArc &arcOrig, CgArc &arcNew, double dAsper, CgBorder &borSubs)
{
	CgBorder bordNew;

	// Riduco arco originale per non arrivare con le REC ad intaccarlo
	double dDelta = dAsper + GetParameters().dIntOverlap;
	if (IsGreater(dDelta, arcOrig.GetRadius() - arcNew.GetRadius()))
		return false;
	CgArc arcReduce(arcOrig);
	arcReduce.SetRadius(arcOrig.GetRadius() - dDelta);

	// La prima parte della nuova spezzata e' identica a quella originale
	// fino alla prima intersezione con arcNew
	CgItem *pItem = borSubs.GetFirstItem();
	bool bFound = false;
	int i;
	for (i = 0; i < borSubs.CountItems() && pItem && !bFound; i++) {
		CgItem *pNew = NULL;
		CbPointArray aInts;
		CgLine *pL = (CgLine *)pItem;
		int iInters = ::Intersect(arcNew, *pL, aInts);
		if (iInters != 0) {
			bFound = true;
			CbPoint ptEnd = aInts[0];
			if (iInters > 1) {
				double dDist = Distance(pItem->GetStart(), ptEnd);
				for (int j = 1; j < iInters; j++) {
					double dd = Distance(pItem->GetStart(), aInts[j]);
					if (dd < dDist)
						ptEnd = aInts[j];
				}
			}

			ASSERT_EXC(pItem->GetType() == CgItem::TYPE_LINE);
			pNew = new CgLine(pItem->GetStart(), ptEnd);

		} else {
			pNew = pItem->Duplicate();
		}
		bordNew.AddItem(pNew);
		pItem = borSubs.GetNextItem();
	}
	if (!bFound)
		return false;

	// L'ultima parte della nuova spezzata e' identica a quella originale
	// a partire dall'ultima intersezione con arcNew
	CgBorder borLast;
	pItem = borSubs.GetLastItem();
	bFound = false;
	for (i = 0; i < borSubs.CountItems() && pItem && !bFound; i++) {
		CbPointArray aInts;
		CgLine *pL = (CgLine *)pItem;
		int iInters = ::Intersect(arcNew, *pL, aInts);
		CgItem *pNew = NULL;
		if (iInters != 0) {
			bFound = true;
			CbPoint ptEnd = aInts[0];
			if (iInters > 1) {
				double dDist = Distance(pItem->GetEnd(), ptEnd);
				for (int j = 1; j < iInters; j++) {
					double dd = Distance(pItem->GetEnd(), aInts[j]);
					if (dd < dDist)
						ptEnd = aInts[j];
				}
			}
			ASSERT_EXC(pItem->GetType() == CgItem::TYPE_LINE);
			pNew = new CgLine(ptEnd, pItem->GetEnd());

		} else {
			pNew = pItem->Duplicate();
		}

		if (pNew != NULL) {
			if (borLast.CountItems() != 0) {
				borLast.GotoFirstItem();
				borLast.InsertItemBefore(pNew);
			} else {
				borLast.AddItem(pNew);
			}
		}

		pItem = borSubs.GetPrevItem();
	}
	ASSERT_EXC(bFound);

	CgLine *pFirst = (CgLine *)(bordNew.GetLastItem());
	CgLine *pLast = (CgLine *)(borLast.GetFirstItem());

	// Cerco di costruire una "scaletta" di elementi che stiano all'interno
	// della corona circolare delimitata da arcReduce e arcNew

	if (IsOverlapping(*pFirst, *pLast)) {
		// Basta modificare pFirst e non inserire pLast
		pFirst->SetEnd(pLast->GetEnd());
		pLast = NULL;
	} else {
		bFound = false;
		// Determino direzione iniziale di ricerca: essa coincide con il multiplo di 90 
		// superiore o uguale a quello della direzione del primo elemento della 
		// spezzata di sostituzione. Considero multiplo uguale solo se direzione e' 
		// gia' parallela agli assi. In effetti calcolo iMul, numero di multipli 
		// di 90 che individuano la direzione di ricerca.
		CbAngle angRic = pFirst->GetAngle();
		double dAng = angRic.Get();
		int iMul = (int)(floor((dAng + resabs) / M_PI_2));
		if (!IsSameValue(dAng, iMul*M_PI_2))
			iMul++;

		bool bEven = true;

		double dDistMin = 0.0;
		while (!bFound) {
			// Individuo direzione del nuovo segmento come a 90 gradi rispetto a 
			// quello precedente
			if (bEven)
				iMul--;
			else
				iMul++;
			bEven = !bEven;

			angRic.Set(iMul*M_PI_2);
			// Determino se si interseca prima acrOrig o arcNew nella direzione 
			// di ricerca
			CbPoint ptStart(pFirst->GetEnd());
			CgInfLine infRic(ptStart, angRic);

			CgLine l;
			l.SetStart(ptStart);
			CbPoint ptPerp;
			CbPoint ptCent = arcNew.GetCenter();
			double dRadCfr = arcNew.GetRadius();

			CbPointArray aInts;
			int iIntersNew = ::Intersect(arcNew, infRic, aInts);
			CbPoint ptWithNew;
			double dDistNew = DBL_MAX;
			bool bIsNewInSearchDir = false;
			if (iIntersNew > 0) {
				ptWithNew = aInts[0];
				dDistNew = Distance(ptWithNew, ptStart);
				if (IsLessEqual(dDistNew, dDistMin)) {
					dDistNew = DBL_MAX;
				} else {
					// Verifico se punto accettabile
					l.SetEnd(ptWithNew);
					ptPerp = l.Perp(ptCent);
					if (!l.Includes(ptPerp)) 
						ptPerp = ptWithNew;
					if (IsGreaterEqual(Distance(ptPerp, ptCent), dRadCfr)) {
						// Punto e' accettabile perche' interno alla corona di roditura
						bIsNewInSearchDir = (l.GetAngle() == angRic);
					} else {
						// Non accettabile
						dDistNew = DBL_MAX;
					}
				}
				for (i = 1; i < iIntersNew; i++) {
					CbPoint pt = aInts[i];
					l.SetEnd(pt);
					double dd = l.GetLen();
					if (IsGreater(dd, dDistMin)) {
						// Verifico se punto accettabile
						ptPerp = l.Perp(ptCent);
						if (!l.Includes(ptPerp)) 
							ptPerp = pt;
						if (IsGreaterEqual(Distance(ptPerp, ptCent), dRadCfr)) {
							bool bDir = (l.GetAngle() == angRic);
							// Punto e' accettabile perche' interno alla corona di roditura
							if ((bDir == bIsNewInSearchDir && IsLess(dd, dDistNew)) ||
								(bDir && !bIsNewInSearchDir)) {
								dDistNew = dd;
								ptWithNew = pt;
								bIsNewInSearchDir = bDir;
							}
						}
					}
				}
			}
			if (IsSameValue(dDistNew, DBL_MAX)) {
				iIntersNew = 0;
				bIsNewInSearchDir = false;
			}

			aInts.RemoveAll();
			int iIntersOrig = ::Intersect(arcReduce, infRic, aInts);
			CbPoint ptWithOrig;
			double dDistOrig = DBL_MAX;
			bool bIsOrigInSearchDir = false;
			if (iIntersOrig > 0) {
				ptWithOrig = aInts[0];
				dDistOrig = Distance(ptWithOrig, ptStart);
				if (IsLessEqual(dDistOrig, dDistMin)) {
					dDistOrig = DBL_MAX;
				} else {
					// Verifico se punto accettabile
					l.SetEnd(ptWithOrig);
					ptPerp = l.Perp(ptCent);
					if (!l.Includes(ptPerp)) 
						ptPerp = ptWithOrig;
					if (IsGreaterEqual(Distance(ptPerp, ptCent), dRadCfr)) {
						// Punto e' accettabile perche' interno alla corona di roditura
						bIsOrigInSearchDir = (l.GetAngle() == angRic);
					} else {
						// Non accettabile
						dDistOrig = DBL_MAX;
					}
				}
				for (i = 1; i < iIntersOrig; i++) {
					CbPoint pt = aInts[i];
					l.SetEnd(pt);
					double dd = l.GetLen();
					if (IsGreater(dd, dDistMin)) {
						// Verifico se punto accettabile
						ptPerp = l.Perp(ptCent);
						if (!l.Includes(ptPerp)) 
							ptPerp = pt;
						if (IsGreaterEqual(Distance(ptPerp, ptCent), dRadCfr)) {
							// Punto e' accettabile perche' interno alla corona di roditura
							bool bDir = (l.GetAngle() == angRic);
							if ((bDir == bIsOrigInSearchDir && IsLess(dd, dDistOrig)) ||
								(bDir && !bIsOrigInSearchDir)) {
								dDistOrig = dd;
								ptWithOrig = pt;
								bIsOrigInSearchDir = bDir;
							}
						}
					}
				}
			}
			if (IsSameValue(dDistOrig, DBL_MAX)) {
				iIntersOrig = 0;
				bIsOrigInSearchDir = false;
			}

			CbPoint ptWithLast;
			double dDistLast = DBL_MAX;
			int iIntersLast = Intersect(*pLast, infRic, ptWithLast, true);
			if (iIntersLast != 0) {
				// Esiste intersezione con  ultimo elemento di borSubs
				// Verifico se punto accettabile
				l.SetEnd(ptWithLast);
				ptPerp = l.Perp(ptCent);
				if (!l.Includes(ptPerp)) 
					ptPerp = ptWithLast;
				if (IsGreaterEqual(Distance(ptPerp, ptCent), dRadCfr)) {
					// Punto e' accettabile perche' interno alla corona di roditura
					dDistLast = l.GetLen();
				} else {
					iIntersLast = 0;
				}
			}

			if (iIntersOrig + iIntersNew + iIntersLast == 0) {
				// Nessuna intersezione trovata: cambio direzione di ricerca
				bEven = !bEven;
			} else {
				if (iIntersLast != 0 &&
						(IsLessEqual(dDistLast, dDistNew) || !bIsNewInSearchDir) && 
						(IsLessEqual(dDistLast, dDistOrig) || !bIsOrigInSearchDir)) {
					// Servono gli "Equal" per preferire la condizione di terminazione del loop.
					// Inoltre preferisco terminare il loop che andare in direzione opposta
					// a quella preferenziale di ricerca
					ptWithNew = ptWithLast;
					pLast->SetStart(ptWithLast);
					bFound = true;
				} else if ((!bIsNewInSearchDir && bIsOrigInSearchDir) ||
							(bIsNewInSearchDir == bIsOrigInSearchDir && 
							 IsLess(dDistOrig, dDistNew))) {
					ptWithNew = ptWithOrig;
					bIsNewInSearchDir = bIsOrigInSearchDir;
				}

				if (!bIsNewInSearchDir) {
					// Ho scelto in direzione opposta a quella preferenziale
					bEven = !bEven;
					iMul += 2;
				}
				pItem = new CgLine(ptStart, ptWithNew);
				bordNew.AddItem(pItem);
				pFirst = (CgLine *)pItem;
			}
		}
	}

	// Sosituisco item di borSubs con quelli trovati
	borSubs.Reset();

	pItem = bordNew.GetFirstItem();
	borSubs.SetAutoCheck(false);
	for (i = 0; i < bordNew.CountItems() && pItem; i++) {
		borSubs.AddItem(pItem);
		pItem = bordNew.GetNextItem();
	}
	bordNew.Reset();

	// Aggiungo items finali (borLast)
	pItem = borLast.GetFirstItem();
	if (pLast)
		borSubs.AddItem(pLast);
	pItem = borLast.GetNextItem();
	for (i = 1; i < borLast.CountItems() && pItem; i++) {
		borSubs.AddItem(pItem);
		pItem = borLast.GetNextItem();
	}

	borLast.Reset();
	borSubs.SetAutoCheck(true);

	return true;
}

//----------------------------------------------------------------------
//
// Description: Spessore della corona circolare utile che si ottiene dalla
//              roditura con utensile tricuspide rotante
//
// Arguments:
//	 dRad -  raggio dell'arco da rodere
//   pTool - utensile
//   dRoughness - asperita` richiesta
//
//----------------------------------------------------------------------
double 
CArcsBorderWorkStrategy::ThickTricuspNib(double dRad, CgToolTricusp *pTool, double dRoughness)
{

	double dRadTool = pTool->GetRadius();

	// Passo angolare delle punzonature
	CbAngle angPitch = CUtilsTools::AngularPitch(dRad, pTool, dRoughness);

	// Raggio di ingombro del punzone ridotto del raggio di smusso delle
	// punte dell'utensile, ovvero distanza fra il centro del punzone e
	// il centro dell'arco di smusso
	double dRadExtRed = pTool->GetExtRadius() - pTool->GetMinRadius();

	// Distanza fra il centro dell'arco da rodere e il centro del punzone
	// posizionato con un lato tangente internamente all'arco da rodere
	double dDistCenters = dRad - dRadTool + dRadExtRed;

	// Distanza fra il centro dell'arco da rodere e i centri degli archi di smusso 
	// delle punte laterali dell'utensile (teorema di Carnot)
	double dDistCP = sqrt(dRadExtRed*dRadExtRed + dDistCenters*dDistCenters - 2.0 * dRadExtRed * dDistCenters * cos(2.0*M_PI/3.0));

	// Angolo fra il vettore che unisce il centro dell'arco da rodere con la punta laterale
	// dell'utensile e il vettore che unisce il centro dell'arco da rodere con il centro
	// dell'utensile (teorema di Carnot)
	double dTmp = (dDistCenters*dDistCenters + dDistCP*dDistCP - dRadExtRed*dRadExtRed) / (2.0*dDistCenters*dDistCP);
	CbAngle angBuff;
	if (fabs(dTmp) > 1.0)
		angBuff = M_PI/1800.0;
	else
		angBuff = acos(dTmp);

	// Angolo fra i vettori che uniscono il centro dell'arco da rodere con le punte laterali
	// delle due punzonature, essendo noto il passo angolare
	CbAngle angExt = angPitch + 2.0 * angBuff;

	// Distanza fra le punte estreme delle due punzonature
	double dDistPP = 2.0 * dDistCP * sin((angExt / 2.0).Get());

	// Semicorda deli archi identificati dall'intersezione delle due circonferenze
	// centrate sulle punte estreme dei due utensili
	double dSemiChord = 0.0;
	if (dRadTool > dDistPP / 2.0)
		dSemiChord = sqrt(dRadTool*dRadTool - (dDistPP / 2.0)*(dDistPP / 2.0));

	// Distanza fra il centro dell'arco da rodere e l'intersezione fra le due
	// circonferenze di cui sopra
	double d = dDistCP * cos((angExt / 2.0).Get()) - dSemiChord;

	// Il risultato finale e` la distanza fra l'intersezione fra le due circonferenze
	// e l'arco da rodere, cioe` lo spessore della corona circolare utile.

	return dRad - d;
}

//----------------------------------------------------------------------
//
// Description: Spessore della corona circolare utile che si ottiene dalla
//              roditura con utensile raggiatore multiplo
//
// Arguments:
//	 dRad -  raggio dell'arco da rodere
//   dRadTool - raggio dell'utensile
//   dRoughness - asperita` richiesta
//
//----------------------------------------------------------------------
double 
CArcsBorderWorkStrategy::ThickMultRadNib(double dRad, CgToolRadMult *pTool, int iIndexRadius, double dRoughness)
{

	CbAngle		angPitch;
	double		dRadTool = pTool->GetRadius(iIndexRadius),
				dRadExt	= pTool->GetExtRadius(),
				dDistCenters,
				dModuleTransl;
						

	// Estraggo l'immagine del punzone
	CgImage *pImageTool = pTool->GetImage();
	if (pImageTool == NULL)
		return 0.0;

	// Dall'immagine mi creo due copie del bordo
	CgBorder border1 (*(pImageTool->GetBorder(0)));
	CgBorder border2 (*(pImageTool->GetBorder(0)));

	// Giro i bordi in base all'indice del raggio di roditura
	CbSignedAngle angRot;
	if (pTool->GetFamily() == CgTool::FAM_RADMULT4)
		angRot = CbSignedAngle(-M_PI_2 * (double)(iIndexRadius+1) + M_PI_2 / 2.0);
	else
		angRot = CbSignedAngle(-M_PI_2 / 2.0 * (double)(iIndexRadius+1) + M_PI_2 / 4.0);
	CbPoint ptCenterTool = NULL_POINT + pTool->GetShift();
	border1.ZRotate(ptCenterTool, angRot);		// border1.GetCenter(), angRot);
	border2.ZRotate(ptCenterTool, angRot);		// border2.GetCenter(), angRot);

	// Calcolo la distanza fra i centri degli archi i cui raggi sono quello esterno
	// del punzone (attualmente tale centro e` nell'origine) e quello di roditura
	// Semicorda dei due archi
	double dSemiChord;
	if (pTool->GetFamily() == CgTool::FAM_RADMULT4)
		dSemiChord = dRadExt * sin(M_PI / 4.0);
	else
		dSemiChord = dRadExt * sin(M_PI / 8.0);
	dDistCenters = sqrt(dRadTool*dRadTool - dSemiChord*dSemiChord) -
				   sqrt(dRadExt*dRadExt - dSemiChord*dSemiChord);

	// Il raggio di roditura puo` essere minore o maggiore del raggio dell'arco
	// da rodere: l'algoritmo e` quindi diverso
	if (IsSameValue(dRadTool, dRad)) 
	{
		// Ampiezza della traslazione
		dModuleTransl = dDistCenters;

		// Passo angolare della roditura
		angPitch = 2.0*asin(dSemiChord/dRadTool);
	}
	else if (IsLess(dRadTool, dRad))
	{
		// Ampiezza della traslazione
		dModuleTransl = dRad - dRadTool + dDistCenters;

		// Passo angolare della roditura
		angPitch = CUtilsTools::AngularPitch(dRad, pTool, dRoughness, iIndexRadius);
	}
	else
	{
		// Semiangolo sotteso dall'utensile: per calcolarlo devo sfruttare il cerchio
		// di ingombro dell'utensile
		double dAngTool;
		if (pTool->GetFamily() == CgTool::FAM_RADMULT4)
			dAngTool = asin(dRadExt * sin(M_PI / 4.0) / dRadTool);
		else
			dAngTool = asin(dRadExt * sin(M_PI / 8.0) / dRadTool);

		// Semiangolo sotteso dall'arco avente raggio pari a dRad e avente la stessa 
		// corda dell'arco dell'utensile
		double dAngSott = asin(dRadTool / dRad * sin(dAngTool));

		// Calcolo la distanza fra i centri degli archi i cui raggi sono quello
		// del punzone e quello del cerchio rodere
		double dDistCenters2 = dRadTool * cos(dAngTool) - dRad * cos(dAngSott);
	
		// Ampiezza della traslazione
		dModuleTransl = dDistCenters - dDistCenters2;

		// Il passo angolare della roditura e` dato da due volte dAngSott
		angPitch = CbAngle(2.0 * dAngSott);
	}

	// Rototraslo i due bordi in modo da simulare le due punzonature a passo
	// angolare calcolato
#if 1
	border1.ZRotate(ptCenterTool, CbSignedAngle((angPitch / 2.0).Get()));
	border2.ZRotate(ptCenterTool, -CbSignedAngle((angPitch / 2.0).Get()));
#endif
	border1.Move(CbVector(dModuleTransl, angPitch / 2.0));
	border2.Move(CbVector(dModuleTransl, 2.0 * M_PI - angPitch / 2.0));
#if 0
	border1.ZRotate(border1.GetCenter(), CbSignedAngle((angPitch / 2.0).Get()));
	border2.ZRotate(border2.GetCenter(), -CbSignedAngle((angPitch / 2.0).Get()));
#endif

	// Trovo le due (devono essere due) intersezioni fra i due bordi; la piu` vicina
	// all'origine e` quella che interessa. Per problemi di precisione nella verifica di appartenenza 
	// di un punto ad un arco, si possono trovare piu' di due intersezioni. Si considera percio'
	// quella piu' vicina all'origine a prescindere dal numero di intersezioni trovate
	if (0) {
		border1.SetPen(2);
		border2.SetPen(2);
		CgCircle cc(NULL_POINT, dRad);
		cc.SetPen(3);
		cc.Show();
		border1.Show();
		border2.Show();
		pISM->WaitForUser();
	}
	CbPointArray apt;
	int iNumOfIntersections = Intersect(border1, border2, apt);
	double dDist1 = Distance(apt[0], CbPoint(0.0, 0.0));
	for (int i=1; i<iNumOfIntersections; i++)
		dDist1 = mymin(dDist1, Distance(apt[i], CbPoint(0.0, 0.0)));

	// Distruggo immagine e bordi
	pImageTool->DeleteAllItems();
	delete pImageTool;
	border1.DeleteAllItems();
	border2.DeleteAllItems();

	return dRad - dDist1;
}

//----------------------------------------------------------------------
//
// Description: Minima asperita` ottenibile in roditura con utensile tricuspide
//              fisso (non in posizione rotante)
//
// Arguments:
//   dRad - raggio dell'arco da rodere
//   pTool - puntatore all'utensile tricuspide
//   dMinDist - distanza minima fra i centri di due punzonature successive in roditura
//
// Remarks:	Minima asperita` ottenibile in roditura con utensile tricuspide
//
//----------------------------------------------------------------------
double 
CArcsBorderWorkStrategy::MinRoughTricusp(double dRad, CgToolTricusp *pTool, double dMinDist)
{

	if (IsSameValue(dMinDist, 0.0))
		return 0.0;

	// Raggio dell'utensile
	double dRadTool = pTool->GetRadius();
	// Raggio di raccordo dell'utensile
	double dRadConn = pTool->GetMinRadius();
	// Raggio della circonferenza che circoscrive l'utensile
	double dRadExt = pTool->GetExtRadius();

	// L'asperita` massima, dato il passo angolare, si ha sicuramente a 60 gradi; in 
	// paricolare si ha fra l'ultimo e il penultimo colpo fatti con l'utensile avente la 
	// "punta verso l'alto" in corrispondenza dei 60 gradi.
	// Per trovare l'asperita` simulo quei due colpi e ne trovo il punto interessante
	// di intersezione in modo da calcolare poi la sua minima distanza dalla 
	// circonferenza che contiene l'arco da rodere.

	// Passo angolare fra i due colpi, approssimato prendendo il cerchio che contiene
	// l'arco di roditura dell'utensile (in questo modo sono anche implicitamente
	// conservativo)
	CbAngle ang (2.0 * asin((dMinDist / 2.0) / (dRad - dRadTool)));

	// Versore che unisce l'origine con i centri degli archi di roditura dell'utensile
	// nelle due posizioni
	CbUnitVector v1 (CbAngle(M_PI / 3.0));
	CbUnitVector v2 (CbAngle(M_PI / 3.0) - ang);

	// Centro dell'arco di roditura dell'utensile all'ultimo colpo
	CbPoint p1 = NULL_POINT + v1 * (dRad - dRadTool);
	// Centro dell'arco di roditura dell'utensile al penultimo colpo
	CbPoint p2 = NULL_POINT + v2 * (dRad - dRadTool);
	// Centro dell'ultimo colpo
	CbPoint c1 = p1 + CbUnitVector(CbAngle(M_PI / 6.0)) * (dRadExt - dRadConn);
	// Centro del penultimo colpo
	CbPoint c2 = p2 + CbUnitVector(CbAngle(M_PI / 6.0)) * (dRadExt - dRadConn);

	// Acquisisco l'immagine del punzone per poi prenderne due bordi
	CgImage *pImageTool = pTool->GetImage();
	if (pImageTool == NULL)
		return DBL_MAX;
	CgBorder b1 (*pImageTool->GetBorder(0));
	CgBorder b2 (*pImageTool->GetBorder(0));
	// Ruoto i bordi in modo che abbiano la punta verso l'alto
	b1.ZRotate(NULL_POINT, CbSignedAngle(-pTool->GetTotalRotation().Get() + M_PI));
	b2.ZRotate(NULL_POINT, CbSignedAngle(-pTool->GetTotalRotation().Get() + M_PI));
	b1.Move(c1 - NULL_POINT);
	b2.Move(c2 - NULL_POINT);

	CbPointArray apt;
	int iInters = Intersect(b1, b2, apt);
	ASSERT_EXC(iInters == 2);
	CbPoint pointRough;
	if (Distance(apt[0], NULL_POINT) > Distance(apt[1], NULL_POINT))
		pointRough = apt[0];
	else
		pointRough = apt[1];
	
	// Distruggo immagine e bordi
	pImageTool->DeleteAllItems();
	delete pImageTool;
	b1.DeleteAllItems();
	b2.DeleteAllItems();

	return dRad - Distance(pointRough, NULL_POINT);
}

//----------------------------------------------------------------------
//
// Description: Minima asperita` ottenibile in roditura con utensile raggiatore
//              multiplo
//
// Arguments:
//   pArc - puntatore all'arco da rodere
//   pTool - puntatore all'utensile raggiatore multiplo
//   dMinDist - distanza minima fra i centri di due punzonature successive in roditura
// 
// Return values: ritorna l'asperita` minima.
//                Inoltre iRadIndex e` l'indice del raggio del tool associato
//                all'asperita` minima
//
// Remarks:	Minima asperita` ottenibile in roditura con utensile raggiatore multiplo
//
//----------------------------------------------------------------------
double 
CArcsBorderWorkStrategy::MinRoughRadMult(CgArc *pArc, CgToolRadMult *pTool, double dMinDist, int &iRadIndex)
{
	double dRad = pArc->GetRadius(),
		   dRadTool;

	if (IsSameValue(dMinDist, 0.0))
		return 0.0;

	// Raggio dell'utensile piu` vicino a quello dell'arco da rodere
	dRadTool = pTool->GetNearestRadius(dRad);
	iRadIndex = pTool->GetNearestIndex(dRad);

	// Se il raggio dell'utensile e` piu` piccolo del raggio dell'arco da rodere
	// l'algoritmo e` lo stesso del caso dell'utensile circolare
	if (IsLessEqual(dRadTool, dRad))
		return CUtilsTools::MinRoughCir(dRad, dRadTool, dMinDist);

	// Altrimenti l'asperita` e` data dalla distanza fra l'arco da rodere
	// e l'arco dell'utensile appoggiato internamente all'arco da rodere.

	// Semiangolo sotteso dall'utensile: per calcolarlo devo sfruttare il cerchio
	// di ingombro dell'utensile
	double dExtRadius = pTool->GetExtRadius();
	double dAngTool;
	if (pTool->GetFamily() == CgTool::FAM_RADMULT4)
		dAngTool = asin(dExtRadius * sin(M_PI / 4.0) / dRadTool);
	else
		dAngTool = asin(dExtRadius * sin(M_PI / 8.0) / dRadTool);	

	// Semiangolo sotteso dall'arco avente raggio pari a dRad e avente la stessa corda
	// dell'arco dell'utensile
	double dAngSott = asin(dRadTool / dRad * sin(dAngTool));

	// Calcolo la distanza fra i centri degli archi i cui raggi sono quello del punzone
	// e quello del cerchio rodere
	double dDistCenters = dRadTool * cos(dAngTool) - dRad * cos(dAngSott);

	// La distanza tra i due archi e` l'asperita`
	return dRad + dDistCenters - dRadTool;
}

//----------------------------------------------------------------------
//
// FUNCTION:	NibbleIsPossible
//
// ARGUMENTS:	arc - arco da rodere
//               pItemPrev - puntatore all'item precedente l'arco
//               pItemNext - puntatore all'item seguente l'arco
//               pTool - raggio dell'utensile
//               dRoughness - asperita` in NIB
//               dRoughnessEnds - asperita` agli estremi della NIB
//
// RETURN VALUE: true se c'e` compatibilita`
//				apEnds - centri del primo e ultimo colpo di punzonatura
//				iRadIndex - indice del raggio del tool da utilizzare (nel caso
//                           di raggiatore multiplo)
//
// AUTHOR(S):    E. Bertuzzi
//
// DATE:			04/98
//
// DESCRIPTION:	Dati l'arco la rodere e l'utensile di roditura, verifica la compatibilita` 
//               dell'utensile con i valori di asperita` richiesti. Opera su archi
//               orientati sia in senso orario sia antiorario (per quelli orientati in
//               senso orario si considerano solo utensili circolari)
//               Nel caso in cui uno o entrambi i puntatori agli item precedente e
//               seguente siano NULL, da` per scontata la compatibilita` con i
//               corrispondenti valori di asperita` agli estremi della NIB
//
//----------------------------------------------------------------------
bool 
CArcsBorderWorkStrategy::NibbleIsPossible(CgArc &arc, CgItem *pItemPrev, CgItem *pItemNext, CgTool *pTool, double dRoughness, double dRoughnessEnds, CbPointArray &apEnds, int &iRadIndex, double &dMinRough, bool bUseSdex)
{
	double dRoughness1, dRoughness2;
	CbPointArray apt;
	double dRadTool, dRadExtTool;
	CbPoint c (arc.GetCenter());
	double  dRad = arc.GetRadius();
	dMinRough = DBL_MAX;
	int iMultRadInd = 0;

	apEnds.RemoveAll();

	switch (pTool->GetFamily())
	{
		case CgTool::FAM_CIRC:
			dRadTool = dRadExtTool = ((CgToolCirc *)pTool)->GetRadius();
			break;
		case CgTool::FAM_RADMULT4:
			iMultRadInd = ((CgToolRadMult4 *)pTool)->GetNearestIndex(dRad);
			dRadTool = ((CgToolRadMult4 *)pTool)->GetRadius(iMultRadInd);
			dRadExtTool = ((CgToolRadMult4 *)pTool)->GetExtRadius();
			break;
		case CgTool::FAM_RADMULT8:
			iMultRadInd = ((CgToolRadMult8 *)pTool)->GetNearestIndex(dRad);
			dRadTool = ((CgToolRadMult8 *)pTool)->GetRadius(iMultRadInd);
			dRadExtTool = ((CgToolRadMult8 *)pTool)->GetExtRadius();
			break;
		case CgTool::FAM_TRICUSP:
			dRadTool = ((CgToolTricusp *)pTool)->GetRadius();
			dRadExtTool = ((CgToolTricusp *)pTool)->GetExtRadius();
			break;
		default :
			return false;
	}	

	// Posiziono il punzone in corrispondenza del punto iniziale dell'arco. Il cerchio
	// associato al punzone ha raggio ridotto di una quantita` pari a CAMPUNCH_OFFSET_BORDER (piu`
	// di resabs perche` le funzioni che calcolano le intersezioni lavorano con
	// la tolleranza resabs e quindi potrebbero sorgente problemi)
	CbUnitVector unitV (arc.GetStartAngle());
	// Si procede in funzione del tipo di arco (orientazione)
	CbPoint c1;
	if (arc.GetSignedAngle() >= 0)
		c1 = c + unitV * (dRad - (dRadExtTool + CAMPUNCH_OFFSET_BORDER));
	else
		c1 = c + unitV * (dRad + dRadExtTool + CAMPUNCH_OFFSET_BORDER);
	CgCircle cirTool (c1, dRadExtTool - CAMPUNCH_OFFSET_BORDER);

	if (pItemPrev != NULL)
	{
		// Verifico l'interferenza con l'item precedente: se c'e` interferenza, trovo 
		// il centro del cerchio che ha lo stesso ingombro dell'utensile e che e` tangente 
		// all'arco da rodere e all'item precedente
		if (pItemPrev->GetType() == CgItem::TYPE_ARC)
		{
			if (Intersect(cirTool, *(CgArc *) pItemPrev, apt, true) != 0)
			{
				CgArc *pA = (CgArc *) pItemPrev;
				// M. Beninca 9/12/2002
				// se l'arco seguente � troppo piccolo come raggio potrebbe essere che con
				// l'utensile raggiatore ci sto lo stesso... uso quindi la corda dell'arco come
				// riferimrnto per trovare la posizione dell'utensile ma solo se l'angolo dell'arco
				// � pi� piccolo di 90 gradi
				if(	pTool->GetFamily() == CgTool::FAM_RADMULT4 
				||	pTool->GetFamily() == CgTool::FAM_RADMULT8 
				||	pTool->GetFamily() == CgTool::FAM_TRICUSP 
				&&	(pA->GetRadius() < cirTool.GetRadius() && pA->GetAngle() < M_PI_2)	)
				{
					CgLine TestLine(pItemPrev->GetStart(), pItemPrev->GetEnd());
					if (!CenterCircleTang(dRadExtTool, TestLine, arc, CAMPUNCH_OFFSET_BORDER, c1))
						return false;
				}
				else
				{
					if (!CenterCircleTang(dRadExtTool, *(CgArc *) pItemPrev, arc, CAMPUNCH_OFFSET_BORDER, c1))
						return false;
				}
			}
		}
		else
		{
			if (Intersect(cirTool, *(CgLine *) pItemPrev, apt, true) != 0)
				if (!CenterCircleTang(dRadExtTool, *(CgLine *) pItemPrev, arc, CAMPUNCH_OFFSET_BORDER, c1))
					return false;
		}
		// Valore dell'asperita` al primo estremo dell'arco. Devo togliere anche CAMPUNCH_OFFSET_BORDER,
		// che e` stato aggiunto nella definizione di c1
		dRoughness1 = Distance(c1, arc.GetStart()) - dRadExtTool - CAMPUNCH_OFFSET_BORDER;
	}
	else
		dRoughness1 = dRoughnessEnds;

	// Posiziono il punzone in corrispondenza del punto finale dell'arco
	unitV = CbUnitVector(arc.GetEndAngle());
	CbPoint c2;
	if (arc.GetSignedAngle() >= 0)
		c2 = c + unitV * (dRad - (dRadExtTool + CAMPUNCH_OFFSET_BORDER));
	else
		c2 = c + unitV * (dRad + dRadExtTool + CAMPUNCH_OFFSET_BORDER);

	cirTool.SetCenter(c2);

	if (pItemNext != NULL)
	{
		// Verifico l'interferenza con l'item seguente: se c'e` interferenza, trovo 
		// il centro del cerchio che ha lo stesso ingombro dell'utensile e che e` tangente 
		// all'arco da rodere e all'item seguente

		if (pItemNext->GetType() == CgItem::TYPE_ARC)
		{
			if (Intersect(cirTool, *(CgArc *) pItemNext, apt, true) != 0)
			{
				CgArc *pA = (CgArc *) pItemPrev;
				// M. Beninca 9/12/2002
				// se l'arco precedente � troppo piccolo come raggio potrebbe essere che con
				// l'utensile raggiatore ci sto lo stesso... uso quindi la corda dell'arco come
				// riferimrnto per trovare la posizione dell'utensile ma solo se l'angolo dell'arco
				// � pi� piccolo di 90 gradi
				if(	pTool->GetFamily() == CgTool::FAM_RADMULT4
				||	pTool->GetFamily() == CgTool::FAM_RADMULT8
				||	pTool->GetFamily() == CgTool::FAM_TRICUSP 
				&&	(pA->GetRadius() < cirTool.GetRadius() && pA->GetAngle() < M_PI_2)	)
				{
					CgLine TestLine(pItemNext->GetStart(), pItemNext->GetEnd());
					if (!CenterCircleTang(dRadExtTool, arc, TestLine, CAMPUNCH_OFFSET_BORDER, c2))
						return false;
				}
				else
				{
					if (!CenterCircleTang(dRadExtTool, arc, *(CgArc *) pItemNext, CAMPUNCH_OFFSET_BORDER, c2))
						return false;
				}
			}
		}
		else
		{
			if (Intersect(cirTool, *(CgLine *) pItemNext, apt, true) != 0)
				if (!CenterCircleTang(dRadExtTool, arc, *(CgLine *) pItemNext, CAMPUNCH_OFFSET_BORDER, c2))
					return false;
		}
		// Valore dell'asperita` al secondo estremo dell'arco. Devo togliere anche CAMPUNCH_OFFSET_BORDER,
		// che e` stato aggiunto nella definizione di c2
		dRoughness2 = Distance(c2, arc.GetEnd()) - dRadExtTool - CAMPUNCH_OFFSET_BORDER;
	}
	else
		dRoughness2 = dRoughnessEnds;

	CbVector v1 (c1 - c);
	CbVector v2 (c2 - c);

	// A questo punto la distanza fra il centro dell'arco da rodere e i due punti c1 e c2
	// deve essere la stessa. Se non e` cosi`, probabilmente non e` stato possibile calcolare
	// tali punti in modo corretto. Inoltre la distanza suddetta deve essere minore del raggio
	// dall'arco se quest'ultimo e` antiorario, deve essere maggiore del raggio dell'arco
	// se e` orario
	if (!IsSameValue(v1.Len(), v2.Len()) ||
		(arc.GetSignedAngle() < 0.0 && (IsLess(v1.Len(), dRad) || IsLess(v2.Len(), dRad))) ||
		(arc.GetSignedAngle() >= 0.0 && (IsGreater(v1.Len(), dRad) || IsGreater(v2.Len(), dRad))))
		return false;

	// L'angolo compreso fra v1 e v2 (per archi antiorari) deve essere minore dell'angolo dell'arco; 
	// se non e` cosi` allora probabilmente l'arco ha ampiezza piccola e gli spostamenti effettuati 
	// con CenterCircleTang hanno addirittura fatto invertire le posizioni di c1 e c2 rispetto al
	// centro c. In tal caso ritorno false
	if ((arc.GetSignedAngle() >= 0 && v2.Angle() - v1.Angle() > arc.GetAngle()) ||
		(arc.GetSignedAngle()  < 0 && v1.Angle() - v2.Angle() > arc.GetAngle()))
			return false;


	// I due centri sono stati calcolati in base al raggio di ingombro, quindi e` necessario
	// farli coincidere con i centri delle punzonature effettive iniziale e finale.
	// Inoltre controllo la compatibilita` fra il valore di asperita` richiesta (parametro
	// del CAM) e l'asperita` minima che dipende dalla geometria e dalla costante
	// macchina che definisce la distanza minima fra i centri di due colpi
	// consecutivi in roditura.
	// Asperita` minima ottenibile: nel caso di utensile raggiatore multiplo
	// il calcolo dell'asperita` e` particolare, in quanto si possono usare per
	// la roditura sia raggi piu` piccoli del raggio dell'arco da rodere, sia 
	// raggi piu` grandi

	double dMinRoughness;
	double dDelta;
	double dMinDistCirRough = GetConstants().dMinDistCirRough;

	switch (pTool->GetFamily())
	{
		case CgTool::FAM_CIRC:
			// precontrollo asperita` minima
			if(IsGreater(dMinDistCirRough / 2.0, dRadTool))
				return false;
			if(!bUseSdex)
				dMinRoughness = CUtilsTools::MinRoughCir(dRad, dRadTool, dMinDistCirRough, arc.GetSignedAngle() >= 0);				
			break;
		case CgTool::FAM_RADMULT4:
			{	
				CgToolRadMult4 *pRadMult4 = (CgToolRadMult4*) pTool;
				// Calcolo la distanza fra i centri degli archi i cui raggi sono quello esterno
				// del punzone e quello di roditura
				double dSemiChord = dRadExtTool * sin(M_PI / 4.0);
				double dCatRadTool = sqrt(dRadTool*dRadTool - dSemiChord*dSemiChord);
				double dCatRadExt = sqrt(dRadExtTool*dRadExtTool - dSemiChord*dSemiChord);
				double dDistCenters = dCatRadTool - dCatRadExt;
				// Distanza tra i due archi
				dDelta = dRadExtTool - (dRadTool - dDistCenters);

				// Tutti i controlli precedenti sono stati fatti assumendo come utensile
				// il cerchio che inscrive l'utensile. Con il raggiatore multiplo devo
				// stare attento che posisiziondo l'utensile vero posso andare ad inteferire
				// con eventuali item precedenti e sucessivi. Anzi questo � sicuro nel caso che
				// questi item formino angoli minori di PI con i vettori tangenti agli estremi
				// dell'arco. Devo quindi correggere la posizione dell'utensile del valore
				// dell'angolo di cui si sborda spostando l'utensile nella sua vera posizione
				// considerando che poi si usa un raggio diverso da quello del cerchio inscritto
				
				c1 = c + (v1 / v1.Len()) * (dRad - dRadExtTool + dDelta - resabs);
				c2 = c + (v2 / v2.Len()) * (dRad - dRadExtTool + dDelta - resabs);

				if(!bUseSdex)
					dMinRoughness = MinRoughRadMult(&arc, (CgToolRadMult4 *)pTool, dMinDistCirRough, iRadIndex);

				// semiangolo sotteso dal raggio usato per lavorare rispetto al
				// raggio dell'arco
				double dHalfAngle = atan2(dSemiChord, dRad);

				// angoli di start ed end corretti dell'arco
				CbAngle StartAngle(v1.Angle().Get() - dHalfAngle);
				CbAngle EndAngle(v2.Angle().Get() + dHalfAngle);

				// M. Beninca 31/03/2003
				// Eseguo test di interferenza con ITEM precedente e sucessivo:
				// per essere sicuro uso le immagini del punzone e non un algoritmo
				// geometrico come prima

				bool bInterNext = false;
				bool bInterPrev = false;

				CgImage *pToolImageNext = pTool->GetImage();
				pToolImageNext->Move(c2 - CbPoint(0,0,0));
				pToolImageNext->ZRotate(c2, pRadMult4->GetRotationToAngle(iMultRadInd, CbSignedAngle(v2.Angle().Get())));
				// Verifico l'interferenza con l'item seguente
				if (pItemNext != NULL && pToolImageNext->GetBorder(0))
				{
					CgBorder intBorder;
					intBorder.AddItem(pItemNext);
					// Eseguo un offset del bordo per non trovare come intersezioni i
					// punti di tangenza
					CgBorder *pOffBorder = NULL;
					CgBorder *pRefBor = pToolImageNext->GetBorder(0);
					double dOff = -3.0*resabs;
					while (pOffBorder == NULL && -dOff > resabs*0.01) {
						dOff = dOff/2.0;
						pOffBorder = pRefBor->Offset(dOff);
					}

					if(pOffBorder)
					{
						bInterNext = ::AreIntersecting(*pOffBorder, intBorder);
						pOffBorder->DeleteAllItems();
						delete pOffBorder;
					}
					else
						bInterNext = false;
					intBorder.Reset();
				}

				CgImage *pToolImagePrev = pTool->GetImage();
				pToolImagePrev->Move(c1 - CbPoint(0,0,0));
				pToolImagePrev->ZRotate(c1, pRadMult4->GetRotationToAngle(iMultRadInd, CbSignedAngle(v1.Angle().Get())));
				// Verifico l'interferenza con l'item precedente
				if (pItemPrev != NULL && pToolImagePrev->GetBorder(0))
				{
					CgBorder intBorder;
					intBorder.AddItem(pItemPrev);
					// Eseguo un offset del bordo per non trovare come intersezioni i
					// punti di tangenza
					CgBorder *pOffBorder = NULL;
					CgBorder *pRefBor = pToolImagePrev->GetBorder(0);
					double dOff = -3.0*resabs;
					while (pOffBorder == NULL && -dOff > resabs*0.01) {
						dOff = dOff/2.0;
						pOffBorder = pRefBor->Offset(dOff);
					}
					if(pOffBorder)
					{
						bInterPrev = ::AreIntersecting(*pOffBorder, intBorder);
						pOffBorder->DeleteAllItems();
						delete pOffBorder;
					}
					else
						bInterPrev = false;
					intBorder.Reset();
				}

				pToolImageNext->DeleteAllItems();
				delete pToolImageNext;
				pToolImageNext = NULL;
				pToolImagePrev->DeleteAllItems();
				delete pToolImagePrev;
				pToolImagePrev = NULL;

				// calcolo gli angoli veri di start ed end della roditura
				// eventualmente corretti per interferenza
				CbAngle c1Angle = v1.Angle();
				CbAngle AngleAdded(0.0);
				if(bInterPrev){
					CbAngle DiffStartAngle = arc.GetStartAngle() - StartAngle;
					c1Angle += DiffStartAngle;
					AngleAdded += DiffStartAngle;
				}
				CbAngle c2Angle = v2.Angle();
				if(bInterNext){
					CbAngle DiffEndAngle = EndAngle - arc.GetEndAngle();
					c2Angle -= DiffEndAngle;
					AngleAdded += DiffEndAngle;
				}

				// se ho ridotto troppo l'intervallo di roditura allora
				// l'utensile non si pu� usare
				if(AngleAdded > (v2.Angle() - v1.Angle()))
					return false;

				// calcolo i centri della prima e l'ultima roditura
				v1 = CbVector(v1.Len(), c1Angle);
				v2 = CbVector(v2.Len(), c2Angle);

				c1 = c + (v1 / v1.Len()) * (dRad - dRadExtTool + dDelta - resabs);
				c2 = c + (v2 / v2.Len()) * (dRad - dRadExtTool + dDelta - resabs);
			}
			break;
		case CgTool::FAM_RADMULT8:
			{	
				CgToolRadMult8 *pRadMult8 = (CgToolRadMult8*) pTool;
				// Calcolo la distanza fra i centri degli archi i cui raggi sono quello esterno
				// del punzone e quello di roditura
				double dSemiChord = dRadExtTool * sin(M_PI / 8.0);
				double dCatRadTool = sqrt(dRadTool*dRadTool - dSemiChord*dSemiChord);
				double dCatRadExt = sqrt(dRadExtTool*dRadExtTool - dSemiChord*dSemiChord);
				double dDistCenters = dCatRadTool - dCatRadExt;
				// Distanza tra i due archi
				dDelta = dRadExtTool - (dRadTool - dDistCenters);

				// Tutti i controlli precedenti sono stati fatti assumendo come utensile
				// il cerchio che inscrive l'utensile. Con il raggiatore multiplo devo
				// stare attento che posisiziondo l'utensile vero posso andare ad inteferire
				// con eventuali item precedenti e sucessivi. Anzi questo � sicuro nel caso che
				// questi item formino angoli minori di PI con i vettori tangenti agli estremi
				// dell'arco. Devo quindi correggere la posizione dell'utensile del valore
				// dell'angolo di cui si sborda spostando l'utensile nella sua vera posizione
				// considerando che poi si usa un raggio diverso da quello del cerchio inscritto
				
				c1 = c + (v1 / v1.Len()) * (dRad - dRadExtTool + dDelta - resabs);
				c2 = c + (v2 / v2.Len()) * (dRad - dRadExtTool + dDelta - resabs);

				if(!bUseSdex)
					dMinRoughness = MinRoughRadMult(&arc, (CgToolRadMult8 *)pTool, dMinDistCirRough, iRadIndex);

				// semiangolo sotteso dal raggio usato per lavorare rispetto al
				// raggio dell'arco
				double dHalfAngle = atan2(dSemiChord, dRad);

				// angoli di start ed end corretti dell'arco
				CbAngle StartAngle(v1.Angle().Get() - dHalfAngle);
				CbAngle EndAngle(v2.Angle().Get() + dHalfAngle);

				// M. Beninca 31/03/2003
				// Eseguo test di interferenza con ITEM precedente e sucessivo:
				// per essere sicuro uso le immagini del punzone e non un algoritmo
				// geometrico come prima

				bool bInterNext = false;
				bool bInterPrev = false;

				CgImage *pToolImageNext = pTool->GetImage();
				pToolImageNext->Move(c2 - CbPoint(0,0,0));
				pToolImageNext->ZRotate(c2, pRadMult8->GetRotationToAngle(iMultRadInd, CbSignedAngle(v2.Angle().Get())));
				// Verifico l'interferenza con l'item seguente
				if (pItemNext != NULL && pToolImageNext->GetBorder(0))
				{
					CgBorder intBorder;
					intBorder.AddItem(pItemNext);
					// Eseguo un offset del bordo per non trovare come intersezioni i
					// punti di tangenza
					CgBorder *pOffBorder = NULL;
					CgBorder *pRefBor = pToolImageNext->GetBorder(0);
					double dOff = -3.0*resabs;
					while (pOffBorder == NULL && -dOff > resabs*0.01) {
						dOff = dOff/2.0;
						pOffBorder = pRefBor->Offset(dOff);
					}
					if(pOffBorder)
					{
						bInterNext = ::AreIntersecting(*pOffBorder, intBorder);
						pOffBorder->DeleteAllItems();
						delete pOffBorder;
					}
					else
						bInterNext = false;
					intBorder.Reset();
				}

				CgImage *pToolImagePrev = pTool->GetImage();
				pToolImagePrev->Move(c1 - CbPoint(0,0,0));
				pToolImagePrev->ZRotate(c1, pRadMult8->GetRotationToAngle(iMultRadInd, CbSignedAngle(v1.Angle().Get())));
				// Verifico l'interferenza con l'item precedente
				if (pItemPrev != NULL && pToolImagePrev->GetBorder(0))
				{
					CgBorder intBorder;
					intBorder.AddItem(pItemPrev);
					// Eseguo un offset del bordo per non trovare come intersezioni i
					// punti di tangenza
					CgBorder *pOffBorder = NULL;
					CgBorder *pRefBor = pToolImagePrev->GetBorder(0);
					double dOff = -3.0*resabs;
					while (pOffBorder == NULL && -dOff > resabs*0.01) {
						dOff = dOff/2.0;
						pOffBorder = pRefBor->Offset(dOff);
					}
					if(pOffBorder)
					{
						bInterPrev = ::AreIntersecting(*pOffBorder, intBorder);
						pOffBorder->DeleteAllItems();
						delete pOffBorder;
					}
					else
						bInterPrev = false;
					intBorder.Reset();
				}

				pToolImageNext->DeleteAllItems();
				delete pToolImageNext;
				pToolImageNext = NULL;
				pToolImagePrev->DeleteAllItems();
				delete pToolImagePrev;
				pToolImagePrev = NULL;

				// calcolo gli angoli veri di start ed end della roditura
				// eventualmente corretti per interferenza
				CbAngle c1Angle = v1.Angle();
				CbAngle AngleAdded(0.0);
				if(bInterPrev){
					CbAngle DiffStartAngle = arc.GetStartAngle() - StartAngle;
					c1Angle += DiffStartAngle;
					AngleAdded += DiffStartAngle;
				}
				CbAngle c2Angle = v2.Angle();
				if(bInterNext){
					CbAngle DiffEndAngle = EndAngle - arc.GetEndAngle();
					c2Angle -= DiffEndAngle;
					AngleAdded += DiffEndAngle;
				}

				// se ho ridotto troppo l'intervallo di roditura allora
				// l'utensile non si pu� usare
				if(AngleAdded > (v2.Angle() - v1.Angle()))
					return false;

				// calcolo i centri della prima e l'ultima roditura
				v1 = CbVector(v1.Len(), c1Angle);
				v2 = CbVector(v2.Len(), c2Angle);

				c1 = c + (v1 / v1.Len()) * (dRad - dRadExtTool + dDelta - resabs);
				c2 = c + (v2 / v2.Len()) * (dRad - dRadExtTool + dDelta - resabs);
			}
			break;
		case CgTool::FAM_TRICUSP:
			dDelta = 2.0 * dRadExtTool - dRadTool - ((CgToolTricusp *)pTool)->GetMinRadius();
			c1 = c + (v1 / v1.Len()) * (dRad - dRadExtTool + dDelta - resabs);
			c2 = c + (v2 / v2.Len()) * (dRad - dRadExtTool + dDelta - resabs);

			// precontrollo asperita` minima
			if(IsGreater(dMinDistCirRough / 2.0, dRadTool))
				return false;
			if(!bUseSdex)
				dMinRoughness = CUtilsTools::MinRoughCir(dRad, dRadTool, dMinDistCirRough);
			break;
	}

	if(bUseSdex){
		// chiedo ad SDEX di calcolaere l'asperit� reale prodotta
		int iRet = CUtilsTools::GetRealRoughForTool(&GET_TOOL_DB, pTool, &arc, dRoughness, dMinRoughness, iRadIndex);
		
		// errore impossible calcolare aperit� o asperit� impossibile da programmare
		if(iRet == 1 || iRet == 2)
			return false;

		// SDEX puo' ritornare asperita' negativa se il raggio usato per la roditura
		// e' maggiore del raggio dell'arco da rodere
		dMinRoughness = fabs(dMinRoughness);
	}
	
	// Ora posso fare il controllo sulle aperita`
	if (IsGreaterEqual(dRoughness, dMinRoughness) && IsLessEqual(dRoughness1, dRoughnessEnds) && IsLessEqual(dRoughness2, dRoughnessEnds))
	{
		// imposto l'asperit� minima
		dMinRough = dMinRoughness;
		apEnds.Add(c1);
		apEnds.Add(c2);
		return true;
	}

	return false;
}

//----------------------------------------------------------------------
//
// FUNCTION:		DoInternalNib
//
// ARGUMENTS:	 c - centro dell'arco
//               dRadMax - raggio dell'arco originale
//               dRadMin - raggio minimo
//               v1 - versore in direzione centro - primo estremo dell'arco
//               v2 - versore in direzione centro - secondo estremo dell'arco
//               border - bordo con la spezzata di sostituzione
//               borOrig - bordo originale del foro
//               pPrevItem - item precedente l'arco
//               pNextItem - item seguente l'arco
//               dDistMin - profondita` del segmento circolare da distruggere
//				iToolType - tipo di utesile richiesto
//				bOnlySingleHit - si richiede la verifica per soli colpi singoli?
//				lWIArc - lista di istruzioni che vengono programmate per risolvere l'arco
//
//
// RETURN VALUE:
//
// AUTHOR(S):    E. Bertuzzi
//
// DATE:
//
// DESCRIPTION:  Esegue la sgrossatura interna di un arco
//
//----------------------------------------------------------------------
bool 
CArcsBorderWorkStrategy::DoInternalNib (CbPoint &c, double dRadMax, double &dRadMin, CbVector &v1, CbVector &v2, 
							 CgBorder &border, CgBorder &borOrig, CgItem *pPrevItem, CgItem *pNextItem, 
							 double dDistMin, int iToolType, bool bOnlySingleHit, 
							 PUNCH_INSTRUCTION_LIST &lWIArc)
{
	double			dRadTool;
	double			dRough;
	int				iNumOfTools, i;
	CgToolCirc		*pToolCirc;
	CgToolRect		*pToolRect;
	CbPointArray	apt;
	CbPoint			c1, c2;
	bool			bNibOk = false;

	// Controllo se posso risolvere con un singolo colpo, con tool circolare o rettangolare
	CgToolArray aToolCirc;
	iNumOfTools = GET_TOOL_DB.FindCirc(aToolCirc, iToolType, CbCond(dRadMin, 0.0, dRadMax - dRadMin), true);
	for (i=0; i<iNumOfTools; i++)
	{
		pToolCirc = (CgToolCirc *)aToolCirc[i];
		if (!CUtilsTools::IsToolUsableInPoint(GET_TOOL_DB, pToolCirc, c))
			continue;

		// Controllo se il tool interferisce con il bordo originale
		CgCircle cirTool (c, pToolCirc->GetRadius());
		CgBorder borTool;
		borTool.AddItem(&cirTool);
		int nNumInt = Intersect(borTool, borOrig, apt);
		borTool.Reset();
		if (nNumInt == 0)
		{
			// Programmazione della punzonatura
			CmXyp *pXyp = new CmXyp(&GETPUNCHPROGRAM);
			pXyp->SetRefPoint(c);
			pXyp->SetUsedTool(pToolCirc);
			lWIArc.AddTail(pXyp);

			return true;
		}
	}
	CgToolArray aToolRect;
	iNumOfTools = GET_TOOL_DB.FindRect(aToolRect, iToolType, CbCond(2.0 * dRadMin, 0.0, 2.0 * (dRadMax - dRadMin)), CbCond(2.0 * dRadMin, 0.0, 2.0 * (dRadMax - dRadMin)), CbCond(), true, false);
	for (i=0; i<iNumOfTools; i++) 
	{
		pToolRect = (CgToolRect *)aToolRect[i];
		if (!CUtilsTools::IsToolUsableInPoint(GET_TOOL_DB, pToolRect, c))
			continue;

		// Controllo se il tool interferisce con il bordo originale
		CgImage *pImageTool = pToolRect->GetImage();
		if (pImageTool == NULL)
			continue;
		CgBorder *pBorTool = pImageTool->GetBorder(0);
		pBorTool->Move(CbVector(c - CbPoint(0,0)));
		if (Intersect(*pBorTool, borOrig, apt) == 0)
		{
			// Programmazione della punzonatura
			CmXyp *pXyp = new CmXyp(&GETPUNCHPROGRAM);
			pXyp->SetRefPoint(c);
			pXyp->SetUsedTool(pToolRect);
			lWIArc.AddTail(pXyp);

			pImageTool->DeleteAllItems();
			delete pImageTool;

			return true;
		}
		pImageTool->DeleteAllItems();
		delete pImageTool;
	}

	if (bOnlySingleHit)
		return false;

	// Non potendo risolvere con un singolo colpo, trovo un utensile che possa
	// fare il giro di sgrossatura senza interagire col bordo originale. I vincoli
	// sulla dimensione minima dell'utensile sono: minimo tra:
	// - raggio maggiore del parametro GetParameters().dMinToolNIB
	// - diametro maggiore dello spessore del foglio
	// (la condizione � la stessa della ArchPunch)
	double dMinDistCirRough = GetConstants().dMinDistCirRough;
	double dMinRadCond = mymax(GetParameters().dMinToolNIB, 2.0 * GetParameters().dThickness);
	iNumOfTools = GET_TOOL_DB.FindCirc(aToolCirc, iToolType, CbCond(dRadMin, dRadMin - dMinRadCond, 0.0), true);
	for (i=0; i<iNumOfTools && !bNibOk; i++)
	{
		pToolCirc = (CgToolCirc *)aToolCirc[i];
		dRadTool = pToolCirc->GetRadius();

		// L'asperita` da impostare per le RIC di sgrossatura la calcolo in base
		// al numero complessivo di colpi da fare.
		dRough = CUtilsTools::BestRoughSgr(dDistMin, dRadMin, pToolCirc, v2.Angle() - v1.Angle());

		// Devo controllare che non sia piu` piccola della minima (moltiplicata per due per
		// non essere proprio al limite) e che non sia piu` grande della differenza fra i 
		// raggi massimo e minimo
		dRough = mymax(dRough, 2.0 * CUtilsTools::MinRoughCir(dRadMin, dRadTool, dMinDistCirRough));
		dRough = mymin(dRough, dRadMax - dRadMin);

		// Se l'asperita` e` piu` grande del raggio dell'utensile esco
		if (dRough > dRadTool)
			return false;

		// In base al valore di asperita` stabilisco il raggio della roditura.
		// Centri iniziale e finale della roditura
		c1 = c + (v1 / v1.Len()) * (dRadMin - dRadTool + dRough);
		c2 = c + (v2 / v2.Len()) * (dRadMin - dRadTool + dRough);

		// Costruisco il profilo della roditura
		CbPolygon *pPolyNib = NULL;
		if(CUtilsGeom::CreatePolyNibbling(&GETPUNCHPROGRAM, pToolCirc, 0, c, c1, c2, dRadMin + dRough, dRough, -CAMPUNCH_OFFSET_BORDER, pPolyNib) && pPolyNib != NULL)
		{
			// Controllo se interagisce col bordo originale
			CgBorder *pOffBor = NULL;
			double dOff = 2.0*resabs;
			while (pOffBor == NULL && dOff > resabs*0.01) {
				dOff = dOff/2.0;
				pOffBor = borOrig.Offset(dOff);
			}
			if (pOffBor == NULL)
				pOffBor = new CgBorder(borOrig);

			CbPolygon *pPolyBorder = pOffBor->GetPolygon(resabs, false);
			bNibOk = CUtilsGeom::IsNibblingContained(*pPolyNib, *pPolyBorder, 0.001);
			delete pPolyNib; pPolyNib = NULL;
			delete pPolyBorder;
			pOffBor->DeleteAllItems();
			delete pOffBor;
		}
		// altrimenti provo a vedere se sono gli archi estremi del profilo che interagiscono
		// col bordo originale. Se e` cosi` sposto verso l'interno tali estremi del profilo
		// di quel tanto che basta affinche` non interferiscano piu` e riprovo a controllare
		// l'interferenza
		if (!bNibOk)
		{
			bool bSkip = false;
			CbPoint cen1 = c1;
			CbPoint cen2 = c2;

			// Cerchi che contengono gli archi estremi del profilo di roditura
			CgCircle cir1 (c1, dRadTool);
			CgCircle cir2 (c2, dRadTool);
			// Bordo che contiene il primo cerchio
			CgBorder bor1;
			bor1.AddItem(&cir1);
			// Bordo che contiene il secondo cerchio
			CgBorder bor2;
			bor2.AddItem(&cir2);

			// Intersezione del primo cerchio col bordo originale
			CbPointArray apt1, apt2, apt3, apt4;
			int iInters1 = Intersect(bor1, borOrig, apt1);
			// Intersezione del secondo cerchio col bordo originale
			int iInters2 = Intersect(bor2, borOrig, apt2);

			// Distruggo elementi nei bordi temporanei.
			bor1.Reset();
			bor2.Reset();

			// Accetto 0 o 2 intersezioni
			if ((iInters1 == 0 || iInters1 == 2) && (iInters2 == 0 || iInters2 == 2))
			{
				if (iInters1 > 0)
				{
					// Retta che passa per i due punti di intersezione
					CgInfLine infL1 (apt1[0], apt1[1]);

					// Trovo il punto di intersezione fra la retta e l'arco centrato in c
					// che circoscrive il cerchio
					CgArc arc1 (c, Distance(c, c1) + dRadTool, CbUnitVector(v1.Angle() - M_PI_2), CbSignedAngle(M_PI));
					iInters1 = Intersect(arc1, infL1, apt3, true);
					// Perche` abbia senso continuare, almeno uno dei punti (al massimo due)
					// di intersezione deve appartenere all'item precedente l'arco. Se le intersezioni 
					// sono due, considero il punto piu` vicino a c1
					bSkip = true;
					CbPoint pointOk;
					double dDist = DBL_MAX;
					for (int j=0; j<apt3.GetSize(); j++)
					{
						if (pPrevItem->Includes(apt3[j]))
						{
							if (Distance(apt3[j], c1) < dDist)
							{
								pointOk = apt3[j];
								dDist = Distance(apt3[j], c1);
							}
							bSkip = false;
						}
					}

					if (!bSkip)
					{
						// Ora mi costruisco un segmento e un arco per poi trovare, per tangenza a
						// questi due item, il nuovo centro del primo arco estremo della banana
						CbVector v = (pointOk - c);
						CgArc arc2 (c, Distance(c, c1) + dRadTool, v, CbSignedAngle(M_PI));
						CbPoint p1;
						if (apt1[0] != pointOk)
							p1 = apt1[0];
						else
							p1 = apt1[1];
						CgLine line (p1, pointOk);

						// Centro del nuovo primo arco estremo della banana. Se si sono problemi, alzo
						// un flag che verra` interpretato come skipping, cioe` passo all'utensile 
						// successivo
						if (CenterCircleTang(dRadTool, line, arc2, CAMPUNCH_OFFSET_BORDER, cen1))
						{
							CbVector vec = (cen1 - c);
							cen1 = c + CbUnitVector(vec.Angle()) * (vec.Len() + CAMPUNCH_OFFSET_BORDER);
						}
						else
							bSkip = true;
					}
				}
				if (!bSkip && iInters2 > 0)
				{
					// Retta che passa per i due punti di intersezione
					CgInfLine infL2 (apt2[0], apt2[1]);

					// Trovo il punto di intersezione fra la retta e l'arco centrato in c
					// che circoscrive il cerchio
					CgArc arc1 (c, Distance(c, c2) + dRadTool, CbUnitVector(v2.Angle() - M_PI_2), CbSignedAngle(M_PI));
					iInters2 = Intersect(arc1, infL2, apt4, true);
					// Perche` abbia senso continuare, almeno uno dei punti (al massimo due)
					// di intersezione deve appartenere all'item seguente l'arco. Se le intersezioni 
					// sono due, considero il punto piu` vicino a c2
					bSkip = true;
					CbPoint pointOk;
					double dDist = DBL_MAX;
					for (int j=0; j<apt4.GetSize(); j++)
					{
						if (pNextItem->Includes(apt4[j]))
						{
							if (Distance(apt4[j], c2) < dDist)
							{
								pointOk = apt4[j];
								dDist = Distance(apt4[j], c2);
							}
							bSkip = false;
						}
					}

					if (!bSkip)
					{
						// Ora mi costruisco un segmento e un arco per poi trovare, per tangenza a
						// questi due item, il nuovo centro del secondo arco estremo della banana
						CbVector v = (pointOk - c);
						CgArc arc2 (c, Distance(c, c2) + dRadTool, CbUnitVector(v.Angle() - M_PI), CbSignedAngle(M_PI));
						CbPoint p1;
						if (apt2[0] != pointOk)
							p1 = apt2[0];
						else
							p1 = apt2[1]; 
						CgLine line (pointOk, p1);

						// Centro del nuovo secondo arco estremo della banana. Se si sono problemi, alzo
						// un flag che verra` interpretato come skipping, cioe` passo all'utensile 
						// successivo
						if (CenterCircleTang (dRadTool, arc2, line, CAMPUNCH_OFFSET_BORDER, cen2))
						{
							CbVector vec = (cen2 - c);
							cen2 = c + CbUnitVector(vec.Angle()) * (vec.Len() + CAMPUNCH_OFFSET_BORDER);
						}
						else
							bSkip = true;
					}
				}
				// Costruisco nuovamente il profilo a banana per controllarne le
				// intersezioni
				if (!bSkip)
				{
					if(CUtilsGeom::CreatePolyNibbling(&GETPUNCHPROGRAM, pToolCirc, 0, c, cen1, cen2, dRadMin + dRough, dRough, -CAMPUNCH_OFFSET_BORDER, 
												pPolyNib) && pPolyNib != NULL)
					{
						// Controllo se interagisce col bordo originale
						CgBorder *pOffBor = NULL;
						double dOff = 2.0*resabs;
						while (pOffBor == NULL && dOff > resabs*0.01) {
							dOff = dOff/2.0;
							pOffBor = borOrig.Offset(dOff);
						}
						if (pOffBor == NULL)
							pOffBor = new CgBorder(borOrig);

						CbPolygon* pPolyBorder = pOffBor->GetPolygon(resabs, false);
						bNibOk = CUtilsGeom::IsNibblingContained(*pPolyNib, *pPolyBorder, 0.001);
						delete pPolyNib; pPolyNib = NULL;
						delete pPolyBorder;
						pOffBor->DeleteAllItems();
						delete pOffBor;
					}
				}
			}
		}

		if (bNibOk)
		{
			// Programmazione della punzonatura
			CmRic *pRic = new CmRic(&GETPUNCHPROGRAM);
			pRic->SetRefPoint(c);
			pRic->SetUsedTool(pToolCirc);
			pRic->SetSlope(CbVector(c1 - c).Angle());
			pRic->SetNibAngle(CbVector(c2 - c).Angle() - CbVector(c1 - c).Angle());
			pRic->SetWidth(dRadMin + dRough);
			pRic->SetRoughness(dRough);
			lWIArc.AddTail(pRic);
		}
	}

	if (!bNibOk)
		return false;

	// Contributo di questa roditura
	double dDeltaDist = 2.0 * dRadTool - 2.0 * dRough;

	dRadMin = dRadMin - dDeltaDist;

	// Aggiorno profondita` dell'arco da distruggere: se non resta nulla, ho finito
	dDistMin = dDistMin - dDeltaDist;
	if (dDistMin < resabs)
		return true;

	// Intersezioni con la spezzata di sostituzione
	CbSignedAngle ang ((v2.Angle() - v1.Angle()).Get());
	CgArc arcMinNib (c, dRadMin, v1, ang);
	CgBorder borArc;
	borArc.AddItem(&arcMinNib);
	int iInters = Intersect(borArc, border, apt);
	borArc.Reset();
	// Il numero di intersezioni deve essere pari oppure uno
	if (iInters != 1 && !IsSameValue((double) iInters / 2.0, floor((double) iInters / 2.0))) 
		return false;

	// Se le intersezioni sono una, significa che l'arco e` tangente alla spezzata di 
	// sostituzione (infatti nel caso di intersezione fra bordi la tangenza da` un solo punto!),
	// e quindi non serve continuare, perche` l'arco e` risolto
	if (iInters == 1)
		return true;

	// Ordinamento angolare dei punti lungo l'arco inferiore del profilo
	// di roditura
	CbPoint pointRef = c + v1;
	CUtilsGeneral::SortByAngle(apt, c, pointRef);

	// Chiamata ricorsiva: creo prima una variabile che sara` la copia del valore del raggio
	// minimo, in quanto, essendo passato per valore, se venisse passato direttamente, verrebbe
	// modificato ad ogni ciclo del for e quindi non va bene
	double dRadRed;
	for (int j=0; j<iInters; j=j+2)
	{
		CbVector vec1 (apt[j] - c);
		CbVector vec2 (apt[j+1] - c);
		dRadRed = dRadMin;
		if (!DoInternalNib(c, dRadMax, dRadRed, CbUnitVector(vec1.Angle()), CbUnitVector(vec2.Angle()), 
						   border, borOrig, pPrevItem, pNextItem, dDistMin, iToolType, false, 
						   lWIArc))
			return false;
	}
	dRadMin = dRadRed;

	return true;
}

//----------------------------------------------------------------------
//
// FUNCTION:	DoExternalNib
//
// ARGUMENTS:	c - centro dell'arco
//               dRad - raggio dell'arco originale
//               dRadExt - raggio minimo
//               v1 - versore in direzione centro - primo estremo dell'arco
//               v2 - versore in direzione centro - secondo estremo dell'arco
//               border - bordo con la spezzata di sostituzione
//               borOrig - bordo originla del foro
//               dDistMin - profondita` del segmento circolare da distruggere
//				iToolType - tipo di utesile
//				lWIArc - lista di istruzioni che vengono programmate per risolvere l'arco
//
// RETURN VALUE:
//
// AUTHOR(S):    E. Bertuzzi
//
// DATE:			05/98
//
// DESCRIPTION:  Esegue la sgrossatura esterna di un arco orario
//
//----------------------------------------------------------------------
bool 
CArcsBorderWorkStrategy::DoExternalNib (CbPoint &c, double dRad, double dRadExt, CbVector &v1, CbVector &v2, 
							 CgBorder &border, CgBorder &borOrig, double dDistMin, int iToolType, 
							 PUNCH_INSTRUCTION_LIST &lWIArc)
{
	bool			bToolFound = false;
	double			dRadTool;
	double			dDeltaDist;
	double			dRough;
	int				iNumOfTools, i;
	CgToolCirc		*pToolCirc;
	CbPointArray	apt;
	CbSignedAngle	ang;
	CbPoint			c1, c2;

	// Trovo un utensile che possa fare il giro di sgrossatura senza interagire col bordo originale
	// Prima cerco utensili che siano stiano all'interno della dDistMin; se non ne trovo (perche`
	// es. dDistMin e piccola), provo con utensili anche piu` grandi

	CgToolArray aToolCirc;
	iNumOfTools = GET_TOOL_DB.FindCirc(aToolCirc, 
								  iToolType, 
								  CbCond(mymax(dDistMin / 2.0 + GetConstants().dWidthCut, GetParameters().dMinToolNIB), 
										 mymax(dDistMin / 2.0 + GetConstants().dWidthCut, GetParameters().dMinToolNIB) - GetParameters().dMinToolNIB, 
										 0.0), 
								  true);
	if (iNumOfTools == 0)
		iNumOfTools = GET_TOOL_DB.FindCirc(aToolCirc, 
									  iToolType, 
									  CbCond(mymax(dDistMin / 2.0 + GetConstants().dWidthCut, GetParameters().dMinToolNIB), 
											 mymax(dDistMin / 2.0 + GetConstants().dWidthCut, GetParameters().dMinToolNIB) - GetParameters().dMinToolNIB, 
											 COND_IGNORE), 
									  true);

	// Loop finche` la parte non e` tutta distrutta
	while (dDistMin > 0)
	{
		for (i=0; i<iNumOfTools; i++)
		{
			pToolCirc = (CgToolCirc *)aToolCirc[i];
			dRadTool = pToolCirc->GetRadius();

			// Come asperita` per la sgrossatura faccio lo stesso calcolo che viene fatto
			// nella sgrossatura di archi orientati in senso antiorario: questa e` una approssimazione
			// ma e` una soluzione che dovrebbe portare un risultato accettabile e non e` molto
			// costosa.
			dRough = CUtilsTools::BestRoughSgr(dDistMin, dRadExt, pToolCirc, v2.Angle() - v1.Angle());

			// Se l'asperita` e` piu grande del raggio del tool,esco
			if (dRough > dRadTool)
				return false;

			// Devo controllare che non sia piu` piccola della minima (moltiplicata per due per
			// non essere proprio al limite) e che non sia piu` grande della differenza fra il 
			// raggio esterno e il raggio dell'arco
			double dMinDistCirRough = GetConstants().dMinDistCirRough;
			dRough = mymax(dRough, 2.0 * CUtilsTools::MinRoughCir(dRadExt + dRadTool - dRough, dRadTool, dMinDistCirRough));
			dRough = mymin(dRough, dRadExt - dRad);

			// In base al valore di asperita` stabilisco il raggio della roditura.
			// Centri iniziale e finale della roditura
			c1 = c + (v1 / v1.Len()) * (dRadExt + dRadTool - dRough);
			c2 = c + (v2 / v2.Len()) * (dRadExt + dRadTool - dRough);

			// Costruisco il profilo della roditura
			//CmPunchBorder borNib;
			bool bNibOk = false;
			CbPolygon* pPolyNib;
			if (CUtilsGeom::CreatePolyNibbling(&GETPUNCHPROGRAM, pToolCirc, 0, c, c1, c2, dRadExt + 2.0 * dRadTool - dRough, 
										dRough, -CAMPUNCH_OFFSET_BORDER, pPolyNib) && pPolyNib != NULL)
			{
				CgBorder *pOffBor = NULL;
				double dOff = 2.0*resabs;
				while (pOffBor == NULL && dOff > resabs*0.01) {
					dOff = dOff/2.0;
					pOffBor = borOrig.Offset(dOff);
				}
				if (pOffBor == NULL)
					pOffBor = new CgBorder(borOrig);

				CbPolygon* pPolyBorder = pOffBor->GetPolygon(resabs, false);
				bNibOk = CUtilsGeom::IsNibblingContained(*pPolyNib, *pPolyBorder, 0.001);
				delete pPolyNib; pPolyNib = NULL;
				delete pPolyBorder;
				pOffBor->DeleteAllItems();
				delete pOffBor;
			}
			if (bNibOk)
			{
				// Programmazione della punzonatura
				CmRic *pRic = new CmRic(&GETPUNCHPROGRAM);
				pRic->SetRefPoint(c);
				pRic->SetUsedTool(pToolCirc);
				pRic->SetSlope(CbVector(c1 - c).Angle());
				pRic->SetNibAngle(CbVector(c2 - c).Angle() - CbVector(c1 - c).Angle());
				pRic->SetWidth(dRadExt + 2.0 * (dRadTool - dRough));
				pRic->SetRoughness(dRough);
				lWIArc.AddTail(pRic);

				bToolFound = true;
				break;
			}
		}

		if (!bToolFound)
			return false;

		// Contributo di questa roditura
		dDeltaDist = 2.0 * dRadTool - 2.0 * dRough;
		// Aggiorno profondita` dell'arco da distruggere e il raggio dell'arco esterno
		dRadExt = dRadExt + dDeltaDist;
		dDistMin = dDistMin - dDeltaDist;
		// Intersezioni con la spezzata di sostituzione
		ang = (CbVector(c2 - c).Angle() - CbVector(c1 - c).Angle()).Get();
		CgArc arcMaxNib (c, dRadExt, v1, ang);
		CgBorder borArc;
		borArc.AddItem(&arcMaxNib);
		int iInters = Intersect(borArc, border, apt);
		borArc.Reset();
		// Il numero di intersezioni deve essere pari oppure uno
		if (iInters != 1 && !IsSameValue((double) iInters / 2.0, floor((double) iInters / 2.0)))
			return false;

		if (iInters > 1)
		{
			// Ordinamento angolare dei punti lungo l'arco inferiore del profilo
			// di roditura
			CUtilsGeneral::SortByAngle(apt, c, c1);

			v1 = apt[0] - c;
			v2 = apt[1] - c;
		}
		// se il numero di intersezioni e` zero, posso uscire direttamente anche se la dDistMin
		// non e` stata ancora completata; puo` essere infatti che la dDistMin si riferisca a
		// una altra parte dell'arco da rodere e ceh sia abbondante per questa parte
		else
			return true;

	}

	return true;
}

//----------------------------------------------------------------------
//
// FUNCTION:		CreateUnresolvedBorder
//
// ARGUMENTS:	arc - arco originale
//				borderSubs - spezzata di sostituzione
//               dRadRedBuff - raggio dell'arco ridotto: la parte compresa fra questo 
//                             arco e l'arco originale  e` risolta
//
// RETURN VALUE:	aborUnresolved - array di bordi delle aree non risolte
//               point - pounto di riferimento dell'area non risolta
//
// AUTHOR(S):    E. Bertuzzi
//
// DATE:
//
// DESCRIPTION:  Costruisce il bordo che identifica l'area compresa fra la spezzata
//               di sostituzione e l'arco ridotto, definito come quell'arco che e`
//               concentrico con arc e ha raggio dRadRedBuff (sicuramente piu` piccolo
//               del raggio di arc)
//               Nota: questo metodo viene chiamato quando un parte dell'arco e` gia`
//               stata risolta, quindi il metodo si aspetta che dRadRedBuff != 0
//               Poiche` non e` banale costruire correttamente tutti i bordi non risolti
//               (in quanto la spezzata di sostituzione puo` avere forme molto diverse)
//               per ora la routine restituisce solo il punto di riferimento dell'area
//               non risolta
//
//----------------------------------------------------------------------
void 
CArcsBorderWorkStrategy::CreateUnresolvedBorder(CgArc &arc, CgBorder &borderSubs, double dRadRedBuff, CgBaseBorderArray &aborUnresolved, CbPoint &point)
{

	ASSERT_EXC(dRadRedBuff != 0);

	// Centro dell'arco originale
	CbPoint c = arc.GetCenter();

	// Costruisco il cerchio ridotto
	CgCircle cir (c, dRadRedBuff);

	// Creo un bordo che contiene l'arco ridotto
	CgBorder borCir;
	borCir.AddItem(&cir);

	// Intersezioni dell'arco con la spezzata: devono essere almeno due e pari
	CbPointArray apt;
	int iIntersections = Intersect(borCir, borderSubs, apt);
	borCir.Reset();
	if (iIntersections < 2 || !IsSameValue((double) iIntersections / 2.0, floor((double) iIntersections / 2.0))) {
		point = c;
		return;
	}

	// Restituisco semplicemente il primo dei punti di intersezione
	point = apt[0];

	// Se le intersezioni sono due prendo l'area compresa fra spezzata e arco contenuto nel
	// cerchio dalla parte dell'arco originale.
	CgLine *pLine1 = (CgLine *)borderSubs.GetFirstItem();
	CgLine *pLine2 = (CgLine *)borderSubs.GetLastItem();
	if (iIntersections == 2)
	{
		
	}
	// altrimenti per ogni coppia, creo un bordo costituito da un tratto di arco e un segmento
	else
	{
		// Ordinamento angolare dei punti
	}
}

//----------------------------------------------------------------------
//
// FUNCTION:	ObroundMng
//
// ARGUMENTS:	border - bordo
//				iToolType - tipo di utensile
//				bUseNibbling - uso utensile in roditura
//				pSavedBorder - puntatore al bordo originale, prima di eventuali modifiche.
//
// RETURN VALUE: 1 se il bordo e` completamente risolto, 0 altrimenti
//               (false anche se il bordo e` parzialmente risolto)
//				-1 aborted
//
// AUTHOR(S):    E. Bertuzzi
//
// DATE:			05/98
//
// DESCRIPTION:  Riconosce, punzona e sostituisce profili ad asola, semiasola, demiasola
//
//----------------------------------------------------------------------
int 
CArcsBorderWorkStrategy::ObroundMng(CgBorder *pborder, int iToolType, bool bUseNibbling, CgBorder *pSavedBorder)
{

	CbPoint			p1, p2, p;
	CbUnitVector	unitVectPrev,
					unitVectNext;
	CgItem			*pCurItem,
					*pPrevItem,
					*pNextItem;
	POSITION		pos;

	bool bBorderWasModified = true;
	bool bAbort = false;
	while (bBorderWasModified) {
		// Devo eseguire piu' loop in quanto ci sono dei casi (per quanto rari) in cui 
		// le sostituzioni eseguite dai moduli chiamati formano delle "asole" da considerare.
		// Queste nuove asole possono contenere degli item collineari, quindi eseguo la
		// JoinAlignedItems.
		bBorderWasModified = false;
		pborder->JoinAlignedItems();

		// Loop sugli item del bordo

		pCurItem = pborder->GetFirstItem();

		for (int i=0; i<pborder->CountItems() && !bAbort; i++)
		{
			if (WAIT_THREAD->IsStopped()) {
				bAbort = true;
				continue;
			}
			int iNumOfItems = pborder->CountItems();

			if (pCurItem->GetType() == CgItem::TYPE_ARC)
			{
				CgArc *pArc = (CgArc*) pCurItem;
				pos = pborder->GetCurrItemPos();

				// Prendo l'item precedente e seguente e le loro posizioni
				pPrevItem = pborder->GetPrevItem();
				POSITION posPrev = pborder->GetCurrItemPos();
				pborder->GetNextItem();
				pNextItem = pborder->GetNextItem();
				POSITION posNext = pborder->GetCurrItemPos();

				// Condizione necessaria per avere una asola, una semiasola o una demiasola 
				// e` che l'item precedente e seguente siano segmenti e siano tangenti all'arco
				bool bArcIsSolved = false;
				if (pPrevItem->GetType() == CgItem::TYPE_LINE &&
					pNextItem->GetType() == CgItem::TYPE_LINE &&
					((CgLine*)pPrevItem)->GetAngle() == (pArc->GetStartTanVect()).Angle() &&
					((CgLine*)pNextItem)->GetAngle() == (pArc->GetEndTanVect()).Angle())
				{

					/////////////////////////////////////////////////////////////////////////////////
					// M. Beninca COMMENTO DEL COMMENTO 
					// La condizione attualmente attiva � quella formalmente corretta ma in alcuni casi
					// � troppo stringente in quanto non vengono riconosciute come asole features
					// asole che in relat� lo sono all'interno di una certa tolleranza. La condizione
					// commentata invece � pi� permissiva ma la contropartita � che vengono riconosciute 
					// come asole delle geometrie che non lo sono affatto:
					// ESEMPIO file GOCCIA.MOD.
					// NEL CASO IN FUTURO QUALCUNO SI LAMENTASSE DEL FATTO CHE ALCUNE GEOMETRIE
					// ASOLANE NON VENGONO RICONOSCIUTE ALLORA SI DOVR� PENSARE AD UNA CONDIZIONE
					// DI RICONOSCIMENTO PI� SOFISTICATA
					// 
					// Se l'arco ha un angolo di 180 gradi puo` far parte di un'asola o una semiasola,
					// altrimenti se ha un angolo di 90 gradi puo` far parte di una demiasola
					if (pArc->GetSignedAngle() == CbSignedAngle(M_PI))
					////////////////
					//// Il Test commentato e' risultato troppo stringente per alcuni particolari e quindi
					//// sostituito da quello seguente.
					//if (pArc->GetSignedAngle()>0 &&
					//	IsSameValue(Distance(pArc->GetStart(),pArc->GetEnd()),2*pArc->GetRadius()))
					///////////
					{
						// Il bordo e` un'asola se gli item del bordo sono 4 e c'e` il secondo
						// arco di ampiezza 180 gradi
						pborder->SetCurrItemPos(posNext);
						CgItem *pItem = pborder->GetNextItem();
						if (iNumOfItems == 4 &&
							pItem->GetType() == CgItem::TYPE_ARC &&
							((CgArc*)pItem)->GetSignedAngle() == CbSignedAngle(M_PI)) {
							double dTmp = 0.0;
							bool bb = PunchObround(*pborder, iToolType, bUseNibbling, dTmp);
							return (bb ? 1 : 0);
						}
						// altrimenti e` una semiasola
						else
						{
							// Poiche` una semiasola e` sempre una parte di un bordo, non e` 
							// possibile che in questa fase tutto il bordo venga risolto con
							// un unico colpo. Quindi, se il tipo di utensile e` incompatibile
							// con l'utilizzo in roditura, esco subito
							if (!bUseNibbling || iToolType != CgTool::TYPE_PUNCH)
								return 0;

							// Chiamata alla routine che cerca di punzonare la semiasola
							if (PunchSemiObround(*pborder, pos, posNext, iToolType, bUseNibbling, pSavedBorder))
							{
								// Modifica del bordo originale inserita nella routine che genera le 
								// punzonature.
								bBorderWasModified = true;
								bArcIsSolved = true;
								pArc = NULL;
							}
						}
					}
					// Controllo se e` una demiasola. Gli item devono essere comunque piu` di 5
					else if (pArc->GetSignedAngle() == CbSignedAngle(M_PI_2) && iNumOfItems > 5)
					{
						// Poiche` una demiasola e` sempre una parte di un bordo, non e` 
						// possibile che in questa fase tutto il bordo venga risolto con
						// un unico colpo. Quindi, se il tipo di utensile e` incompatibile
						// con l'utilizzo in roditura, esco subito
						if (!bUseNibbling || iToolType != CgTool::TYPE_PUNCH)
							return 0;

						// Affinche` ci sia una demiasola, gli items precedente e seguente devono essere 
						// segmenti tangenti (controllo gia` fatto), l'item che segue l'item seguente 
						// deve essere un arco anch'esso di 90 gradi e deve essere seguito da un segmento 
						// tangente.
						// Mi procuro prima i due puntatori che mancano affinche` tutti e cinque gli items
						// coinvolti siano puntati
						pborder->SetCurrItemPos(posNext);
						CgItem *pItemFourth = pborder->GetNextItem();
						CgItem *pItemFifth = pborder->GetNextItem();
						if (pItemFourth->GetType() == CgItem::TYPE_ARC &&
							((CgArc*)pItemFourth)->GetSignedAngle() == CbSignedAngle(M_PI_2) &&
							pItemFifth->GetType() == CgItem::TYPE_LINE &&
							IsSameVector(((CgLine*)pNextItem)->GetDir(), ((CgArc*)pItemFourth)->GetStartTanVect()) &&
							IsSameVector(((CgLine*)pItemFifth)->GetDir(), ((CgArc*)pItemFourth)->GetEndTanVect()) &&
							IsSameValue(pArc->GetRadius(), ((CgArc*)pItemFourth)->GetRadius()))
						{
							CgArc *pArc2 = (CgArc *) pItemFourth;

							// Chiamata alla routine che cerca di punzonare la demiasola: se la routine
							// restituisce 0 non faccio nulla, se restituisce 1 sostituisco il due archi
							// (che sono stati punzonati da un utensile ad asola) con i due segmenti
							// ortogonali che uniscono gli estremi di ciascun arco con il proprio centro;
							// se restituisce 2, allora la demiasola e` completamente risolta e la
							// sostituisco con un unico segmento
							int iResult = PunchDemiObround(*pborder, pos, iToolType, 
														   bUseNibbling, pSavedBorder);
							if (iResult == 1)
							{
								// Sostituzione primo arco
								CgLine *pLine1 = new CgLine(pArc->GetStart(), pArc->GetCenter());
								CgLine *pLine2 = new CgLine(pArc->GetCenter(), pArc->GetEnd());
								pborder->SetCurrItemPos(pos);
								{
									CgBorder MyBor;
									MyBor.AddItem(pLine1);
									MyBor.AddItem(pLine2);
									pborder->ReplaceItem(&MyBor, true);
									pArc = NULL;
									MyBor.Reset();
								}

								// Sostituzione secondo arco
								pLine1 = new CgLine(pArc2->GetStart(), pArc2->GetCenter());
								pLine2 = new CgLine(pArc2->GetCenter(), pArc2->GetEnd());
								pborder->GetNextItem();
								pborder->GetNextItem();
								{
									CgBorder MyBor;
									MyBor.AddItem(pLine1);
									MyBor.AddItem(pLine2);
									pborder->ReplaceItem(&MyBor, true);
									pArc2 = NULL;
									pNextItem = NULL;
									MyBor.Reset();
								}

								// Sposto il puntatore di position sull'item corrente, che e`
								// il secondo segmento di sostituzione del secondo arco
								pos = pborder->GetCurrItemPos();

								bBorderWasModified = true;
								bArcIsSolved = true;
							}
							else if (iResult == 2)
							{
								// Creo la spezzata (e` un solo segmento)
								CgLine *pLine = new CgLine(pArc->GetStart(), pArc2->GetEnd());

								// Sostituzione della demiasola nel bordo con la spezzata.
								pborder->SetCurrItemPos(pos);
								pborder->ReplaceItems(3, pLine, true);
								pArc = NULL;
								pArc2 = NULL;
								pItemFourth = NULL;
								pNextItem = NULL;
								pos = pborder->GetCurrItemPos();
								bBorderWasModified = true;
								bArcIsSolved = true;
							}
						}
					}
				}
				// Controllo se e` una demiasola "ridotta", cioe' composta da due archi, di cui almeno 
				// uno con angolo superiore a 90^ con segmento in mezzo, tangente ad entrambi. In tal caso
				// potrebbe essere conveniente risolvere con asola (se non ho utensile rettangolo ad hoc.
				// Gli item devono essere comunque piu` di 3.
				// Un angolo pari a M_PI si accetta solo se non definisce una semiasola (la cui
				// soluzione e' conveniente).
				if (!bArcIsSolved && pArc->GetSignedAngle() == M_PI) {
					if (pPrevItem->GetType() == CgItem::TYPE_LINE) {
						if (IsSameVector(((CgLine*)pPrevItem)->GetDir(), pArc->GetStartTanVect()))
							bArcIsSolved = true;
					}
				}

				if (!bArcIsSolved && iNumOfItems > 3)
				{
					// Poiche` una demiasola e` sempre una parte di un bordo, non e` 
					// possibile che in questa fase tutto il bordo venga risolto con
					// un unico colpo. Quindi, se il tipo di utensile e` incompatibile
					// con l'utilizzo in roditura, esco subito
					if (!bUseNibbling || iToolType != CgTool::TYPE_PUNCH)
						return 0;

					// Affinche` ci sia una demiasola "ridotta", l'item seguente deve essere un
					// segmento tangente, l'item che segue l'item seguente 
					// deve essere un arco anch'esso >= 90 gradi avente stesso raggio dell'arco in esame
					// e tangente all'item seguente.
					// Mi procuro prima il puntatore che manca affinche` tutti e tre gli items
					// coinvolti siano puntati.
					pborder->SetCurrItemPos(posNext);
					CgItem *pItemThird = pborder->GetNextItem();
					POSITION posThird = pborder->GetCurrItemPos();
					CgItem *pNextThird = pborder->GetNextItem();
					if (pArc->GetSignedAngle() >= CbSignedAngle(M_PI_2) && 
						pNextItem->GetType() == CgItem::TYPE_LINE &&
						IsSameVector(((CgLine*)pNextItem)->GetDir(), pArc->GetEndTanVect()) &&
						pItemThird->GetType() == CgItem::TYPE_ARC &&
						IsSameVector(((CgLine*)pNextItem)->GetDir(), ((CgArc*)pItemThird)->GetStartTanVect()) &&
						IsSameValue(pArc->GetRadius(),((CgArc*)pItemThird)->GetRadius()) &&
						((CgArc*)pItemThird)->GetSignedAngle() >= CbSignedAngle(M_PI_2))
					{
						// Verifico se pItemThird non definisce una semiasola.
						if (((CgArc*)pItemThird)->GetSignedAngle() == CbSignedAngle(M_PI) &&
							pNextThird->GetType() == CgItem::TYPE_LINE &&
							IsSameVector(((CgLine*)pNextThird)->GetDir(), ((CgArc*)pItemThird)->GetEndTanVect()))
								bArcIsSolved = true;

						if (!bArcIsSolved) {
							// Chiamata alla routine che cerca di punzonare la demiasola: se la routine
							int iResult = PunchDemiObround(*pborder, pos, iToolType, 
														   bUseNibbling, pSavedBorder);
							if (iResult != 0) {
								// La demiasola e' stata risolta: sostituisco i due archi preferibilmente 
								// prolungando elementi adiacenti fino ad incontrare la retta passante per 
								// centro e punto in comune con segmento risolto dalla demiasola. Se cio' non 
								// e' possibile, utilizzo i due segmenti che dai loro estremi vanno al centro.

								// Sostituzione primo arco
								CgLine MyLin(pArc->GetCenter(),pArc->GetEnd());
								bool bIsSubs = false;
								if (pPrevItem->GetType() == CgItem::TYPE_ARC) {
									CgArc *pArcPrev = (CgArc *)pPrevItem;
									CbPointArray apt;
									int nNumInt = Intersect(MyLin, *pArcPrev, apt, false);
									if (nNumInt != 0) {
										// Verifico quale intersezione e' piu' vicina a pArc->GetEnd.
										CbPoint Pint = apt[0];
										if (nNumInt == 2 && IsLess(Distance(pArc->GetStart(), apt[1]),
																   Distance(pArc->GetStart(), Pint)))
											Pint = apt[1];
										if (IsLessEqual(Distance(Pint, pArc->GetCenter()),pArc->GetRadius())) {
											pborder->SetCurrItemPos(posPrev);
											{
												CgBorder MyBor;
												CgArc *pMyArc = new CgArc(pArcPrev->GetCenter(), pArcPrev->GetStart(), 
														Pint, (pArcPrev->GetSignedAngle()>0));
												MyBor.AddItem(pMyArc);
												CgLine *pMyLin = new CgLine(Pint, pArc->GetEnd());
												MyBor.AddItem(pMyLin);
												pborder->ReplaceItems(2, &MyBor, true);
												pArc = NULL;
												pArcPrev = NULL;
												pPrevItem = NULL;
												MyBor.Reset();
												bArcIsSolved = true;
											}
											bIsSubs = true;
										}
									}
								}
								else {
									CgLine *pLinPrev = (CgLine *)pPrevItem;
									CbPoint Pint;
									if (Intersect(MyLin, *pLinPrev, Pint, false) != 0) {
										if (IsLessEqual(Distance(Pint, pArc->GetCenter()),pArc->GetRadius())) {
											pborder->SetCurrItemPos(pos);
											{
												CgBorder MyBor;
												CgLine *pMyLin1 = new CgLine(pLinPrev->GetEnd(), Pint);
												MyBor.AddItem(pMyLin1, false);
												CgLine *pMyLin = new CgLine(Pint, pArc->GetEnd());
												MyBor.AddItem(pMyLin, false);
												pborder->ReplaceItem(&MyBor, true);
												pArc = NULL;
												MyBor.Reset();
												bArcIsSolved = true;
											}
											bIsSubs = true;
										}
									}
								}
								if (!bIsSubs) {
									CgLine *pLine1 = new CgLine(pArc->GetStart(), pArc->GetCenter());
									CgLine *pLine2 = new CgLine(pArc->GetCenter(), pArc->GetEnd());
									pborder->SetCurrItemPos(pos);
									{
										CgBorder MyBor;
										MyBor.AddItem(pLine1);
										MyBor.AddItem(pLine2);
										pborder->ReplaceItem(&MyBor, true);
										pArc = NULL;
										MyBor.Reset();
										bArcIsSolved = true;
									}
								}

								// Sostituzione secondo arco
								CgArc *pArc2 = (CgArc *)pItemThird;
								pborder->SetCurrItemPos(posThird);
								CgItem *pNextNext = pborder->GetNextItem();
								POSITION posNextNext = pborder->GetCurrItemPos();

								MyLin.SetStart(pArc2->GetCenter());
								MyLin.SetEnd(pArc2->GetStart());
								bIsSubs = false;
								if (pNextNext->GetType() == CgItem::TYPE_ARC) {
									CgArc *pArcNext = (CgArc *)pNextNext;
									CbPointArray apt;
									int nNumInt = Intersect(MyLin, *pArcNext, apt, false);
									if (nNumInt != 0) {
										// Verifico quale intersezione e' piu' vicina a pArc->GetEnd.
										CbPoint Pint = apt[0];
										if (nNumInt == 2 && IsLess(Distance(pArc2->GetEnd(), apt[1]),
																   Distance(pArc2->GetEnd(), Pint)))
											Pint = apt[1];
										if (IsLessEqual(Distance(Pint, pArc2->GetCenter()),pArc2->GetRadius())) {
											pborder->SetCurrItemPos(posThird);
											{
												CgBorder MyBor;
												CgLine *pMyLin = new CgLine(pArc2->GetStart(),  Pint);
												MyBor.AddItem(pMyLin);
												CgArc *pMyArc = new CgArc(pArcNext->GetCenter(), Pint, 
														pArcNext->GetEnd(), (pArcNext->GetSignedAngle()>0));
												MyBor.AddItem(pMyArc);
												pborder->ReplaceItems(2, &MyBor, true);
												pArc2 = NULL;
												pItemThird = NULL;
												pNextNext = NULL;
												pArcNext = NULL;
												MyBor.Reset();
											}
											bIsSubs = true;
										}
									}
								}
								else {
									CgLine *pLinNext = (CgLine *)pNextNext;
									CbPoint Pint;
									if (Intersect(MyLin, *pLinNext, Pint, false) != 0) {
										if (IsLessEqual(Distance(Pint, pArc2->GetCenter()),pArc2->GetRadius())) {
											pborder->SetCurrItemPos(posThird);
											{
												CgBorder MyBor;
												CgLine *pMyLin = new CgLine(pArc2->GetStart(), Pint);
												MyBor.AddItem(pMyLin);
												CgLine *pMyLin1 = new CgLine(Pint, pLinNext->GetStart());
												MyBor.AddItem(pMyLin1);
												pborder->ReplaceItem(&MyBor, true);
												pArc2 = NULL;
												pItemThird = NULL;
												MyBor.Reset();
											}
											bIsSubs = true;
										}
									}
								}
								if (!bIsSubs) {
									CgLine *pLine1 = new CgLine(pArc2->GetStart(), pArc2->GetCenter());
									CgLine *pLine2 = new CgLine(pArc2->GetCenter(), pArc2->GetEnd());
									pborder->SetCurrItemPos(posThird);
									{
										CgBorder MyBor;
										MyBor.AddItem(pLine1);
										MyBor.AddItem(pLine2);
										pborder->ReplaceItem(&MyBor, true);
										pArc2 = NULL;
										pItemThird = NULL;
										MyBor.Reset();
									}
								}

								bBorderWasModified = true;
							}
						}
					}

					if (!bArcIsSolved) {
						pborder->SetCurrItemPos(pos);
						CgItem *pPrevious = pborder->GetPrevItem();
						if (pPrevious && pPrevious->GetType() == CgItem::TYPE_LINE &&
							((CgLine*)pPrevious)->GetAngle() == (pArc->GetStartTanVect()).Angle())
						{
							// Verifico se sono in presenza di una asola "troncata", cioe' il bordo ha degli altri archi 
							// appartenenti allo stesso cerchio di quello corrente e tali per cui ricado nel caso di
							// semiasola

							pborder->SetCurrItemPos(pos);
							CgItem *pNext = pborder->GetNextItem();
							POSITION posLoop = pborder->GetCurrItemPos();
							bool bSimilarArc = false;
							while (posLoop && posLoop != pos && !bSimilarArc) {
								pNext = pborder->GetNextItem();
								posLoop = pborder->GetCurrItemPos();
								if (pNext->GetType() == CgItem::TYPE_ARC) {
									CgArc *pArc1 = (CgArc *)pNext;
									if (IsSamePoint(pArc->GetCenter(), pArc1->GetCenter()) &&
										IsSameValue(pArc->GetRadius(), pArc1->GetRadius()) &&
										pArc->GetSignedAngle().Get()*pArc1->GetSignedAngle().Get() > 0.0) { 
										// Trovato archi concentrici
										CgArc arcCompount(pArc->GetCenter(), pArc->GetStart(), pArc1->GetEnd(),!pArc->IsClockwise(), pArc->GetNormal());
										if (arcCompount.GetSignedAngle() == CbSignedAngle(M_PI)) {
											CgItem *pNewNext = pborder->GetNextItem();
											if (pNewNext->GetType() == CgItem::TYPE_LINE &&
												((CgLine*)pNewNext)->GetAngle() == (pArc1->GetEndTanVect()).Angle()) {
												// Chiamata alla routine che cerca di punzonare la semiasola
												POSITION posNextLine = pborder->GetCurrItemPos();
												if (PunchSemiObround(*pborder, pos, posNextLine, iToolType, bUseNibbling, pSavedBorder))
												{
													// Modifica del bordo originale inserita nella routine che genera le 
													// punzonature.
													bSimilarArc = true;
													bBorderWasModified = true;
													bArcIsSolved = true;
													pArc = NULL;
												}
											} 
											pborder->SetCurrItemPos(posLoop);
										}
									}
								}
							}
						}

					}
				}

				// Riprendo la posizione dell'arco o dell'ultimo segmento della spezzata
				// di sostituzione per andare avanti
				pborder->SetCurrItemPos(pos);
			}
			pCurItem = pborder->GetNextItem();

		}
	}
	// Se sono arrivato in fondo, sicuramente il bordo non e` risolto o e` risolto
	// parzialmente, quindi ritorno false
	return (bAbort ? -1 : 0);
}

//----------------------------------------------------------------------
//
// FUNCTION:		PunchObround
//
// ARGUMENTS:	border - bordo ad asola
//				iToolType - tipo di utensile
//				bUseNibbling - uso utensile in roditura
//				dLineMax - lunghezza del segmento che definisce la semiasola piu' lungo
//							(mentre border corrisponde a quello piu' corto).
//						   In uscita vale la lunghezza "lineare" rosa dalla istruzione programmata.
//				bForSemiObr - true se si sta lavorando per semiasole
//				dLenMax - lunghezza massima dell'asola in caso di semiasole
//
// RETURN VALUE: true se la punzonatura e` completata, false altrimenti
//
// AUTHOR(S):    E. Bertuzzi
//
// DATE:			05/98
//
// DESCRIPTION:  Punzonatura di un bordo ad asola
//
//----------------------------------------------------------------------
bool 
CArcsBorderWorkStrategy::PunchObround(CgBorder &border, int iToolType, bool bUseNibbling, double &dLineMax, 
									  bool bForSemiObr, double dLenMax)
{
#ifdef COMPILING_CAMPUNCH_DLL
	CCAD *pCurrCAD = pCamPunchCAD;
#endif
#ifdef COMPILING_CAM_COMBI_DLL
	CCAD *pCurrCAD = pCmbCadHndlr;
#endif

	double			dWidth,
					dLenLines,
					dHeight;
	CgToolObround	*pToolObr;
	CgToolCirc		*pToolCirc;
	CbPoint			c1, c2, c;

	// Ricavo i dati del bordo ad asola

	CgItem *pItem = border.GetFirstItem();
	if (pItem->GetType() == CgItem::TYPE_LINE)
		pItem = border.GetNextItem();
	dHeight = 2.0 * ((CgArc *) pItem)->GetRadius();
	c1 = ((CgArc *) pItem)->GetCenter();
	pItem = border.GetNextItem();
	dLenLines = ((CgLine *) pItem)->GetLen();
	dWidth = dLenLines + dHeight;
	CbAngle angRot = ((CgLine *) pItem)->GetDir().Angle();
	CbUnitVector u (angRot);
	c = c1;

	// Controllo se c'e` il punzone esatto
	CgToolArray aObrTools;

	CbAngle angRic(angRot);
	if (angRic >= M_PI)
		angRic = angRic - M_PI;
	if (IsSameAngle(angRic,2*M_PI))
		angRic = 0.0;

	// gestione approssimazione percentuale utensili
	double dMaxCirAppW = GetParameters().dMaxCirApp;
	if(GetParameters().bUsePercMaxCirApp)
		dMaxCirAppW = dWidth * GetParameters().dPercMaxCirApp / 100.0;
	double dMaxCirAppH = GetParameters().dMaxCirApp;
	if(GetParameters().bUsePercMaxCirApp)
		dMaxCirAppH = dHeight * GetParameters().dPercMaxCirApp / 100.0;

	double dMinCirAppW = GetParameters().dMinCirApp;
	if(GetParameters().bUsePercMinCirApp)
		dMinCirAppW = dWidth * GetParameters().dPercMinCirApp / 100.0;
	double dMinCirAppH = GetParameters().dMinCirApp;
	if(GetParameters().bUsePercMinCirApp)
		dMinCirAppH = dHeight * GetParameters().dPercMinCirApp / 100.0;

	// Verifico presenza di utensile circolare "esatto": in tal caso accetto solo
	// asole che eseguono esattamente il raggio
	double dSearchDist = 0.05/pCurrCAD->GetUnitScale();
	int iNumOfTools = GET_TOOL_DB.FindCirc(aObrTools, iToolType, CbCond(dHeight*0.5, dSearchDist, dSearchDist), true);
	if (iNumOfTools > 0) {
		dMaxCirAppH = dSearchDist;
		dMinCirAppH = dSearchDist;
	}

	iNumOfTools = GET_TOOL_DB.FindObround(aObrTools, iToolType, 
										 CbCond(dWidth, dMinCirAppW, dMaxCirAppW), 
										 CbCond(dHeight, dMinCirAppH, dMaxCirAppH), 
										 CbCond(angRic.Get(), 0.0, 0.0, resnor), false, false);
	if (iNumOfTools == 0 && GetParameters().bObroundOrt)
	{
		// Permesso utilizzo asole in SQR.
		angRic += M_PI_2;
		iNumOfTools = GET_TOOL_DB.FindObround(aObrTools, iToolType, 
										 CbCond(dWidth, dMinCirAppW, dMaxCirAppW), 
										 CbCond(dHeight, dMinCirAppH, dMaxCirAppH), 
										 CbCond(angRic.Get(), 0.0, 0.0, resnor), false, false);
		if (iNumOfTools==0)
			angRic -= M_PI_2;
	}

	int iTool;
	for (iTool=0; iTool<iNumOfTools; iTool++)
	{
		pToolObr = (CgToolObround *)aObrTools[iTool];
		
		// Verifico se utensile e' utilizzabile in questa posizione.
		if (!CUtilsTools::IsToolUsableInPoint(GET_TOOL_DB, pToolObr, border.GetCenter()))
			continue;

		// Programmo la punzonatura
		CmXyp *pXyp = new CmXyp(&GETPUNCHPROGRAM);
		pXyp->SetRefPoint(border.GetCenter());
		pXyp->SetUsedTool(pToolObr);
		// Se e` rotante devo programmare l'angolo corretto
		if (pToolObr->IsRotating() || 
			(pToolObr->GetTotalRotation() != angRot && pToolObr->GetTotalRotation() != angRot-M_PI))
			pXyp->SetToolAngle(angRot - pToolObr->GetTotalRotation());
		if (iToolType == CgTool::TYPE_PUNCH)
			GETPUNCHPROGRAM.SAVE_PUNCH_OPERATION(pXyp, &border);
		else {
			if (pToolObr->GetEmbCycle()==1) 
				pXyp->GetOpt().SetPunOpt(PO_DR1);
			else if (pToolObr->GetEmbCycle()==2) 
				pXyp->GetOpt().SetPunOpt(PO_DR2);
			else if (pToolObr->GetEmbCycle()==3) 
				pXyp->GetOpt().SetPunOpt(PO_DR3);
			else if (pToolObr->GetEmbCycle()==4) 
				pXyp->GetOpt().SetPunOpt(PO_DR4);

			GETPUNCHPROGRAM.SAVE_PUNCH_OPERATION(pXyp, &border);
		}

		if (bForSemiObr)
			dLineMax = dLenLines;

		return true;
	}

	// Se non c'e` l'utensile esatto, controllo se c'e` un'asola avente la stessa altezza ma base 
	// piu` piccola, a meno che il tipo di utensile richiesto non sia incompatibile con l'uso
	// in roditura, nel qual caso esco subito
	if (!bUseNibbling || iToolType != CgTool::TYPE_PUNCH)
		return false;

	iNumOfTools = 0;
	if (bForSemiObr && IsGreater (dLenMax+dHeight, dWidth)) {
		iNumOfTools = GET_TOOL_DB.FindObround(aObrTools, 
										 iToolType, 
										 CbCond(dWidth, 0.0, dLenMax+dHeight-dWidth), 
										 CbCond(dHeight, dMinCirAppH, dMaxCirAppH), 
										 CbCond(angRic.Get(), 0.0, 0.0, resnor), 
										 true, 
										 false);

		if (iNumOfTools == 0 && GetParameters().bObroundOrt)
		{
			// Permesso utilizzo asole in SQR.
			angRic += M_PI_2;
			iNumOfTools = GET_TOOL_DB.FindObround(aObrTools, iToolType, 
										 CbCond(dWidth, 0.0, dLenMax+dHeight-dWidth), 
										 CbCond(dHeight, dMinCirAppH, dMaxCirAppH), 
										 CbCond(angRic.Get(), 0.0, 0.0, resnor), 
										 true, 
										 false);
			if (iNumOfTools==0)
				angRic -= M_PI_2;
		}

	}

	if (iNumOfTools==0) {
		// Nel caso di asole oppure semiasole senza utensile piu' grande, cerco piu' piccolo.
		iNumOfTools = GET_TOOL_DB.FindObround(aObrTools, 
										 iToolType, 
										 CbCond(dWidth, dWidth, 0.0), 
										 CbCond(dHeight, dMinCirAppH, dMaxCirAppH), 
										 CbCond(angRic.Get(), 0.0, 0.0, resnor), 
										 true, 
										 false);
		if (iNumOfTools == 0 && GetParameters().bObroundOrt)
		{
			// Permesso utilizzo asole in SQR.
			angRic += M_PI_2;
			iNumOfTools = GET_TOOL_DB.FindObround(aObrTools, iToolType, 
										 CbCond(dWidth, dWidth, 0.0), 
										 CbCond(dHeight, dMinCirAppH, dMaxCirAppH), 
										 CbCond(angRic.Get(), 0.0, 0.0, resnor), 
										 true, 
										 false);
			if (iNumOfTools==0)
				angRic -= M_PI_2;
		}
	}
	// Se c'e`, il primo e` il migliore (a meno che non sia accettabile per sovrapposizione).
	for (iTool =0; iTool < iNumOfTools; iTool++)
	{
		pToolObr = (CgToolObround *)aObrTools[iTool];

		// Lunghezza del lato rettilineo del tool
		double dSegmTool = pToolObr->GetWidth() - pToolObr->GetHeight();

		// Centro della prima punzonatura
		CbPoint p1 = c + u * (pToolObr->GetWidth() / 2.0 - dHeight / 2.0);

		// Verifico se utensile e' utilizzabile in questa posizione.
		if (!CUtilsTools::IsToolUsableInPoint(GET_TOOL_DB, pToolObr, p1))
			continue;

		if (IsGreaterEqual(dSegmTool, dLenLines) && (!bForSemiObr || IsGreaterEqual(dSegmTool,dLineMax) ||
			IsGreater(dLineMax-dSegmTool,dSegmTool))) {
			// Programmo la punzonatura a meno che non convenga fare STR di due colpi per rodere 
			// semiasola completa.
			CmXyp *pXyp = new CmXyp(&GETPUNCHPROGRAM);
			pXyp->SetRefPoint(p1);
			pXyp->SetUsedTool(pToolObr);
			// Se e` rotante devo programmare l'angolo corretto
			//if (pToolObr->IsRotating())
			if (pToolObr->IsRotating() || 
				(pToolObr->GetTotalRotation() != angRot && pToolObr->GetTotalRotation() != angRot-M_PI))
				pXyp->SetToolAngle(angRot - pToolObr->GetTotalRotation());
			GETPUNCHPROGRAM.SAVE_PUNCH_OPERATION(pXyp, &border);

			if (bForSemiObr)
				dLineMax = dSegmTool;
		}
		else {
			// Calcolo il numero di punzonature e l'interasse fra le stesse, riducendo
			// della sovrapposizione interna la lunghezza dell'utensile in modo da essere conservativo.
			// Ovviamente il parametro di sovrapposizione interna non deve essere troppo
			// alto rispetto alla dimensione dell'utensile
			if (IsLessEqual(dSegmTool, GetParameters().dIntOverlap))
				continue;
			int iNumOfPunch = (int) ceil((dLenLines) / (dSegmTool - GetParameters().dIntOverlap));
			if (bForSemiObr && IsGreater(dLineMax, dLenLines)) {
				// Verifico se conviene punzonare fino al segmento piu' lungo.
				int iN = (int) ceil((dLineMax) / (dSegmTool - GetParameters().dIntOverlap));
				if (iN - iNumOfPunch <= 1) {
					iNumOfPunch = iN;
					dLenLines = dLineMax;
				}
			}
			double dInterAxis= (dLenLines - dSegmTool) / (iNumOfPunch - 1);
			//if (IsLess(dInterAxis,GetParameters().dMinScrapRec)) {
			// Richiesta di G. Rossetto: utilizzare sovrapposizione interna per definire 
			// il passo minimo in quanto utilizzo dello sfrido minimo in REc, pur essendo in via 
			// di principio corretto, non e' intuitivo per l'utente.
			if (IsLess(dInterAxis,GetParameters().dIntOverlap)) {
				// Sovrapposizione troppo stretta.
				continue;
			}
			// Programmo la punzonatura
			CmStr *pStr = new CmStr(&GETPUNCHPROGRAM);
			pStr->SetRefPoint(p1);
			pStr->SetUsedTool(pToolObr);
			// Se e` rotante devo programmare l'angolo corretto
			//if (pToolObr->IsRotating())
			if (pToolObr->IsRotating() || 
				(pToolObr->GetTotalRotation() != angRot && pToolObr->GetTotalRotation() != angRot-M_PI))
				pStr->SetToolAngle(angRot - pToolObr->GetTotalRotation());
			pStr->SetSlope(angRot);
			pStr->SetPunchStep(dInterAxis);
			pStr->SetPunchNumber(iNumOfPunch);
			GETPUNCHPROGRAM.SAVE_PUNCH_OPERATION(pStr, &border);

			if (bForSemiObr)
				dLineMax = dLenLines;
		}

		return true;
	}

	if (IsLess(dLineMax,dLenLines))
		dLineMax = dLenLines;

	// Se non e` risolta, controllo se c'e` l'utensile circolare avente il diametro
	// pari all'altezza del profilo ad asola.
	CgToolArray aCirTools;
	if (IsLess(dHeight / 2.0, GetParameters().dMinToolNIB))
		iNumOfTools = 0;
	else{
		// gestione approssimazione percentuale utensili
		if(GetParameters().bUsePercMaxCirApp)
			dMaxCirAppH = dHeight / 2.0 * GetParameters().dPercMaxCirApp / 100.0;
		if(GetParameters().bUsePercMinCirApp)
			dMinCirAppH = dHeight / 2.0 * GetParameters().dPercMinCirApp / 100.0;

		iNumOfTools = GET_TOOL_DB.FindCirc(aCirTools, iToolType, 
			CbCond(dHeight / 2.0, dMinCirAppH, dMaxCirAppH), true);
	}
	// Se c'e`, il primo e` il migliore
	if (iNumOfTools != 0)
	{
		double	dRoughness,
				dRadTool;
		int		iNumOfPunchCir = 0,
				iNumOfPunchRec = 0;

		pToolCirc = (CgToolCirc *)aCirTools[0];
		dRadTool = pToolCirc->GetRadius();
		
		// Controllo se c'e` un utensile rettangolare che possa distruggere l'interno dell'asola
		CgToolArray	aRectTools;
		CgToolRect	*pToolRect;
		int			iNumOfRecTools,
					iToolRectOk = -1,
					iNumRecOk = -1;
		double		dUToUse, dVToUse, dMaxInterAxis, dInterAxis, dWidthTool, dHeightTool, dAngRotTool;
		bool		bAdd90, bAdd90Ok, bCirIsBest, bIsRot;

		double dDimMax;
		if (bForSemiObr) {
			dDimMax = mymax(dLenMax, dLineMax);
		} else {
			dDimMax = dLineMax;
		}
		iNumOfRecTools = GET_TOOL_DB.FindRect(aRectTools, iToolType, 
			CbCond(dLenLines, dLenLines, (dDimMax-dLenLines)), CbCond(dHeight, dHeight, 0.0), 
			CbCond(angRot.Get(), 0.0, 0.0, resnor), true, false);

		for (int i = 0; i < iNumOfRecTools; i++)
		{
			pToolRect = (CgToolRect *)aRectTools[i];

			bIsRot = pToolRect->IsRotating();
			dWidthTool = pToolRect->GetWidth();
			dHeightTool = pToolRect->GetHeight();
			dAngRotTool = pToolRect->GetTotalRotation().Get();

			iNumOfPunchRec = CUtilsTools::PutRectToolInArea(bIsRot,
											   dWidthTool,
											   dHeightTool,
											   dAngRotTool,
											   dLenLines, 
											   dDimMax, 
											   dHeight, 
											   dHeight, 
											   angRot, 
											   0.0,
											   dUToUse, 
											   dVToUse, 
											   bAdd90);

			if (iNumOfPunchRec > 0 && iNumRecOk < 0)
			{
				iNumRecOk = iNumOfPunchRec;
				iToolRectOk = i;
			}
			else if (iNumOfPunchRec > 0 && iNumOfPunchRec <= iNumRecOk)
			{
				iNumRecOk = iNumOfPunchRec;
				iToolRectOk = i;
				bAdd90Ok = bAdd90;
			}
		}

		// Se il flag "possibilita` di rodere profili ad asola con utensili circolari" e` true,
		// allora controllo quanti colpi sono necessari, tenendo conto dell'asperita` richiesta,
		// per poi confrontarli con i colpi che fa un eventuale utensile rettangolare. Se il
		// circolare non si puo` usare per la roditura e se non c'e` il rettangolare buono,
		// lascio perdere: se il circolare c'e`, verra` poi usato per risolvere singolarmente
		// gli archi nella fase successiva
		if (GetParameters().bObroundWithNIB)
		{
			// L'asperita` richiesta puo` essere espressa in termini assoluti o percentuali 
			// sul raggio dell'arco. Poiche` in questo caso non ho un arco da rodere,
			// l'eventuale valore percentuale lo considero sul raggio dell'utensile
			if (GetParameters().bPercRoughNIB)
				dRoughness = dRadTool * GetParameters().dPercRoughNIB / 100.0;
			else
				dRoughness = GetParameters().dRoughNIB;

			// Calcolo dell'interasse delle punzonature per avere l'asperita` richiesta
			dMaxInterAxis = 2.0 * sqrt(dRadTool*dRadTool - (dRadTool - dRoughness) * (dRadTool - dRoughness));
			
			// Numero di punzonature complessive necessarie
			iNumOfPunchCir = (int) ceil((dLenLines) / dMaxInterAxis) + 1;

			// Interasse effettivo
			dInterAxis = dLenLines / (iNumOfPunchCir - 1);

			// Se c'era almeno un rettangolare che andava bene, confronto il numero di colpi,
			// togliendo, per il circolare, il primo e l'ultimo perche` non sono parte della
			// roditura ma servono per distruggere i due archi dell'asola
			if (iToolRectOk >= 0 && iNumOfPunchCir - 2 >= iNumRecOk) {
				bCirIsBest = false;
			}
			else {
				bCirIsBest = true;
				int iNumCirMax = (int) ceil((dLineMax) / dMaxInterAxis) + 1;
				if (iNumCirMax < iNumOfPunchCir+3) {
					iNumOfPunchCir = iNumCirMax;
					dLenLines = dLineMax;
					dInterAxis = dLenLines / (iNumOfPunchCir - 1);
				} else {
					dLineMax = dLenLines;
				}
			}
		}
		// altrimenti, se c'era almeno un rettangolare che andava bene, uso quello per
		// distruggere l'interno dell'asola
		else if (iToolRectOk >= 0) 
			bCirIsBest = false;

		// Se la parte interna dell'asola puo` essere distrutta con un circolare o
		// un rettangolare, programmo la punzonatura
		if (GetParameters().bObroundWithNIB || iToolRectOk >= 0)
		{
			// Se va bene il circolare per la roditura programmo una STR, altrimenti 
			// programmo dei colpi singoli per i due archi dell'asola piu` una REC
			if (bCirIsBest)
			{
				// Programmo la punzonatura
				CmStr *pStr = new CmStr(&GETPUNCHPROGRAM);
				pStr->SetRefPoint(c);
				pStr->SetUsedTool(pToolCirc);
				pStr->SetSlope(angRot);
				pStr->SetPunchStep(dInterAxis);
				pStr->SetPunchNumber(iNumOfPunchCir);
				GETPUNCHPROGRAM.SAVE_PUNCH_OPERATION(pStr, &border);

				return true;
			}
			else if (!bForSemiObr)
			{
				// Programmazione prima XYP
				CmXyp *pXyp1 = new CmXyp(&GETPUNCHPROGRAM);
				pXyp1->SetRefPoint(c);
				pXyp1->SetUsedTool(pToolCirc);
				GETPUNCHPROGRAM.SAVE_PUNCH_OPERATION(pXyp1, &border);

				// Programmazione seconda XYP
				CbPoint c1 = c + u * (dLenLines);
				CmXyp *pXyp2 = new CmXyp(&GETPUNCHPROGRAM);
				pXyp2->SetRefPoint(c1);
				pXyp2->SetUsedTool(pToolCirc);
				GETPUNCHPROGRAM.SAVE_PUNCH_OPERATION(pXyp2, &border);

				// ricavo l'utensile rettangolare
				pToolRect = (CgToolRect *)aRectTools[iToolRectOk];

				// Programmazione REC
				CbPoint p = c + CbUnitVector(angRot - M_PI_2) * (dHeight / 2.0);
				CmRec *pRec = new CmRec(&GETPUNCHPROGRAM);
				pRec->SetRefPoint(p);
				pRec->SetUsedTool(pToolRect);
				pRec->SetSlope(angRot);
				pRec->SetWidth(dLenLines);
				pRec->SetHeight(dHeight);
				GETPUNCHPROGRAM.SAVE_PUNCH_OPERATION(pRec, &border);
				
				return true;
			}
		}
	}

	return false;
}

//----------------------------------------------------------------------
//
// FUNCTION:		PunchSemiObround
//
// ARGUMENTS:	border - bordo da lavorare
//              posArc - posizione dell'arco della semiasola nel bordo
//				posNextLine - posizione del segmento seguente l'arco (potrebbe non essere il seguiente nel bordo se
//							  la semiasola e' troncata, cioe' l'arco e' costituito da piu' porzioni separate. In tal caso
//							  l'arco in posizione posArc deve essere il primo delle porzioni da considerare)
//				iToolType - tipo di utensile
//				bUseNibbling - uso utensile in roditura
//				pSavedBorder - puntatore al bordo originale, prima di eventuali modifiche.
//
// RETURN VALUE: true se la punzonatura e` completata, false altrimenti
//
// AUTHOR(S):    E. Bertuzzi
//
// DATE:			05/98
//
// DESCRIPTION:  Punzonatura di una semiasola di un bordo
//
//----------------------------------------------------------------------
bool 
CArcsBorderWorkStrategy::PunchSemiObround(CgBorder &border, POSITION &posArc, POSITION posNextLine, int iToolType, 
										  bool bUseNibbling, CgBorder *pSavedBorder)
{
#ifdef COMPILING_CAMPUNCH_DLL
	CCAD *pCurrCAD = pCamPunchCAD;
#endif
#ifdef COMPILING_CAM_COMBI_DLL
	CCAD *pCurrCAD = pCmbCadHndlr;
#endif

	border.SetCurrItemPos(posArc);
	CgArc *pArc = (CgArc *) border.GetCurrItem();
	double dRad = pArc->GetRadius();
	CbPoint c = pArc->GetCenter();

	// Lunghezza del segmento piu` corto fra i due segmenti paralleli della semiasola
	CgLine *pLine = (CgLine *) border.GetPrevItem();
	double dLineLen = pLine->GetLen();
#if 0
	border.GetNextItem();
	pLine = (CgLine *) border.GetNextItem();
#else
	border.SetCurrItemPos(posNextLine);
	pLine = (CgLine *)border.GetCurrItem();
	border.SetCurrItemPos(posArc);
	border.GetNextItem();
#endif

	// Memorizzo anche la lunghezza max tra le due linee, in maniera che poi verifichero' se conviene
	// eseguire l'asola piu' lunga per rodere contemporaneamente tutta la linea piu' lunga.
	double dLineMax = mymax(dLineLen, pLine->GetLen());

	dLineLen = mymin(dLineLen, pLine->GetLen());
	
	// Inclinazione della semiasola
	CbAngle ang = pLine->GetDir().Angle();

	// Ora costruisco un bordo ad asola dalla semiasola, controllo che tale bordo non
	// interferisca col bordo originale, e poi lo risolvo utilizzando la routine
	// che punzona le asole. Infine allineo le strutture in modo che gli statement
	// agganciati al bordo fittizio passino al bordo originale.
	// Costruisco intanto un bordo con offset, per controllare le intersezioni
	CgBorder borObr;
	CgArc	arc1 (c, dRad - CAMPUNCH_OFFSET_BORDER, pArc->GetStartVect(), CbSignedAngle(M_PI));
	CgLine	line1 (arc1.GetEnd(), dLineLen, ang);
	CgArc	arc2 (c + CbUnitVector(ang) * dLineLen, dRad - CAMPUNCH_OFFSET_BORDER, -(pArc->GetStartVect()), CbSignedAngle(M_PI));
	CgLine	line2 (arc2.GetEnd(), arc1.GetStart());
	borObr.SetAutoCheck(false);
	borObr.AddItem(&arc1);
	borObr.AddItem(&line1);
	borObr.AddItem(&arc2);
	borObr.AddItem(&line2);

	CbPointArray apt;
	int nNumInt = Intersect(borObr, *pSavedBorder, apt);
	borObr.Reset();
	if (nNumInt != 0) {
		dLineLen -= dRad + CAMPUNCH_OFFSET_BORDER;
		borObr.SetAutoCheck(false);
		borObr.AddItem(&arc1);
		CgLine line1bis(arc1.GetEnd(), dLineLen, ang);
		borObr.AddItem(&line1bis);
		CgArc arc2bis(c + CbUnitVector(ang) * dLineLen, dRad - CAMPUNCH_OFFSET_BORDER, -(pArc->GetStartVect()), CbSignedAngle(M_PI));
		borObr.AddItem(&arc2bis);
		CgLine line2bis(arc2.GetEnd(), arc1.GetStart());
		borObr.AddItem(&line2bis);
		nNumInt = Intersect(borObr, *pSavedBorder, apt);
		borObr.Reset();
		if (nNumInt != 0) {
			return false;
		}
	}

	// Verifico se e' possibile arrivare fino alla max lunghezza tra linee parallele che definiscono 
	// la semiasola.
	double dLenPunch = dLineMax;
	if (IsGreater(dLineMax, dLineLen)) {
		borObr.Reset();
		line1.SetEnd(line1.GetStart() + CbVector(dLineMax,ang));
		arc2.SetCenter(c + CbVector(dLineMax, ang));
		line2.SetStart(arc2.GetEnd());
		borObr.SetAutoCheck(false);
		borObr.AddItem(&arc1);
		borObr.AddItem(&line1);
		borObr.AddItem(&arc2);
		borObr.AddItem(&line2);
		if (Intersect(borObr, *pSavedBorder, apt) != 0)
			dLenPunch = 0.0;
	}
	else
		dLenPunch = 0.0;

	borObr.Reset();

	// Visto che il bordo con offset non interferisce, costruisco il bordo ad
	// asola con dimensioni esatte e poi lo passo alla routine che punzona l'asola
	CAMBORDER borObrExact;
	CgArc arcExact1 (c, dRad, pArc->GetStartVect(), CbSignedAngle(M_PI));
	CgLine	lineExact1 (arcExact1.GetEnd(), dLineLen, ang);
	CgArc	arcExact2 (c + CbUnitVector(ang) * dLineLen, dRad, arc2.GetEndVect(), CbSignedAngle(M_PI));
	CgLine	lineExact2 (arcExact2.GetEnd(), arcExact1.GetStart());
	borObrExact.SetAutoCheck(false);
	borObrExact.AddItem(&arcExact1);
	borObrExact.AddItem(&lineExact1);
	borObrExact.AddItem(&arcExact2);
	borObrExact.AddItem(&lineExact2);

	// Poiche' sto esaminando una semiasola, si deve calcolare la lunghezza max dell'asola compatibile 
	// con il bordo (noto che quella minima e' data da borObrExact). Questo perche' si deve utilizzare 
	// un'asola anche se essa punzona piu' del minimo necessario.
	double dLenMax = 100.0/pCurrCAD->GetUnitScale();
	CgLine line1Max(arc1.GetEnd(), dLenMax, ang);
	CgArc	arc2Max (c + CbUnitVector(ang) * dLenMax, dRad - CAMPUNCH_OFFSET_BORDER, arc2.GetEndVect(), CbSignedAngle(M_PI));
	CgLine	line2Max (arc2Max.GetEnd(), arc1.GetStart());

	CgBorder borObrMax;
	borObrMax.SetAutoCheck(false);
	borObrMax.AddItem(&arc1);
	borObrMax.AddItem(&line1Max);
	borObrMax.AddItem(&arc2Max);
	borObrMax.AddItem(&line2Max);
	int nInts = Intersect(borObrMax, *pSavedBorder, apt);
	while (nInts != 0 && IsGreater(dLenMax,dLineLen)) {
		for (int k=0; k<nInts; k++) {
			dLenMax = mymin(dLenMax,Distance(apt[k],CgLine(arc1.GetStart(), arc1.GetEnd())));
		}
		dLenMax -= CAMPUNCH_OFFSET_BORDER;
		borObrMax.Reset();
		line1Max.SetStart(arc1.GetEnd());
		line1Max.SetEnd(arc1.GetEnd() + CbVector(dLenMax, ang));
		arc2Max.SetCenter(c + CbUnitVector(ang) * dLenMax);
		line2Max.SetStart(arc2Max.GetEnd());
		line2Max.SetEnd(arc1.GetStart());
		borObrMax.SetAutoCheck(false);
		borObrMax.AddItem(&arc1);
		borObrMax.AddItem(&line1Max);
		borObrMax.AddItem(&arc2Max);
		borObrMax.AddItem(&line2Max);
		nInts = Intersect(borObrMax, *pSavedBorder, apt);
	}

	borObrMax.Reset();

	if (!PunchObround(borObrExact, iToolType, bUseNibbling, dLenPunch, true, dLenMax)) {
		borObrExact.Reset();
		return false;
	}

	// Allineamento statements-bordi
	PUNCH_INSTRUCTION_LIST* pWIListLocal = borObrExact.GET_PUNCH_OPERATIONS;
	for (POSITION pos = pWIListLocal->GetHeadPosition(); pos != NULL; )
	{
		PUNCH_INSTRUCTION *pWILocal = pWIListLocal->GetNext(pos);
		PUNCH_INSTRUCTION_LIST* pWIList = ((CAMBORDER*)&border)->GET_PUNCH_OPERATIONS;
		pWIList->AddTail(pWILocal);
		// Aggiungo il bordo corrente alla lista dei bordi associati
		// alla macro che punzona l'asola e tolgo da questa lista
		// il bordo fittizio
		CgBaseBorderArray* pBorArrayLocal = pWILocal->GetWorkedBorders();
		pBorArrayLocal->RemoveAt(0);
		pBorArrayLocal->Add(&border);
	}

	// Devo modificare il bordo per tenere conto delle punzonature inserite.
	if (IsGreater(dLenPunch, dLineLen)) {
		// Modifico il bordo borObrExact per averlo pari alla punzonatura.
		borObrExact.Reset();
		lineExact1.SetEnd(arcExact1.GetEnd() + CbVector(dLenPunch, ang));
		arcExact2.SetCenter(c + CbVector(dLenPunch, ang));
		lineExact2.SetStart(arcExact2.GetEnd());
		borObrExact.AddItem(&arcExact1);
		borObrExact.AddItem(&lineExact1);
		borObrExact.AddItem(&arcExact2);
		borObrExact.AddItem(&lineExact2);
	}

	border.SetCurrItemPos(posArc);
	border.GetNextItem();
	POSITION posRealNext = border.GetCurrItemPos();

	if (posRealNext != posNextLine) {
		// Sto eseguendo una semiasola troncata: sostituisco tutti gli spezzoni di arco
		POSITION posLoop = posArc;
		border.SetCurrItemPos(posLoop);
		CgArc *pArcOrig = (CgArc *)pArc->Duplicate();
		while (posLoop && posLoop != posNextLine) {
			CgItem *pLoopItem = border.GetCurrItem();
			if (pLoopItem->GetType() == CgItem::TYPE_ARC)
			{
				CgArc *pLoopArc = (CgArc *)pLoopItem;
				if (pLoopArc == pArc ||
					(pLoopArc->GetCenter() == pArcOrig->GetCenter() && 
					 IsSameValue(pLoopArc->GetRadius(), pArcOrig->GetRadius()) &&
					 pLoopArc->IsClockwise() == pArcOrig->IsClockwise())) {
						 // L'arco pLoopArc in posizione posLoop deve essere sostituito
							double			dMaxDist;
							CAMBORDER	borderSubs;

							// Creazione della spezzata di sostituzione
							if (CUtilsGeom::CreatePolylineSubs(border, posLoop, GetParameters().dIntOverlap, borderSubs, dMaxDist) == false)
							{
								// distruzione spezzata sostituzione
								borderSubs.DeleteAllItems();
								
								// creazione URA
								CmUra* pUra = new CmUra(&GETPUNCHPROGRAM, border.GetCenter());
								GETPUNCHPROGRAM.SAVE_PUNCH_OPERATION(pUra, &border);
								((CAMBORDER*)(&border))->SetResolved(true);
							} else {

								// Assegno alla spezzata lo stesso layer dell'arco
								borderSubs.SetLayer(pArcOrig->GetLayer());
						
								// Riprendo la posizione dell'arco
								border.SetCurrItemPos(posLoop);

								// Sostituzione dell'arco con la spezzata
								border.GetNextItem();
								posLoop = border.GetCurrItemPos();
								border.GetPrevItem();
								border.ReplaceItem(&borderSubs, true);
								border.SetCurrItemPos(posLoop);
								border.GetPrevItem();
								posLoop = border.GetCurrItemPos();
								if (pLoopArc == pArc)
									posArc = posLoop;
							}
				}
			}
			border.GetNextItem();
			posLoop = border.GetCurrItemPos();
		}
		pArcOrig->DecRef();
		return true;
	}

	// Prendo l'item precedente e seguente e le loro posizioni
	border.SetCurrItemPos(posArc);
	CgItem *pPrevItem = border.GetPrevItem();
	POSITION posPrev = border.GetCurrItemPos();
	CgItem *pFatherPrevItem;
	POSITION posFatherPrev;
	if (IsLessEqual(((CgLine *)pPrevItem)->GetLen(),dLenPunch)) {
		// Ha senso considerare il precedente del precedente.
		pFatherPrevItem = border.GetPrevItem();
		posFatherPrev = border.GetCurrItemPos();

	} else {
		pFatherPrevItem = pPrevItem;
		posFatherPrev = posPrev;
	}

	border.SetCurrItemPos(posNextLine);
	CgItem *pNextItem = border.GetCurrItem();
	POSITION posNext = posNextLine;
	CgItem *pSonNextItem;
	POSITION posSonNext;
	if (IsLessEqual(((CgLine *)pNextItem)->GetLen(),dLenPunch)) {
		// Ha senso considerare il seguente del seguente
		pSonNextItem = border.GetNextItem();
		posSonNext = border.GetCurrItemPos();
	} else {
		pSonNextItem = pNextItem;
		posSonNext = posNext;
	}

	// La sostituzione controlla innanzitutto i due item "esterni" alla semiasola, cioe' pFatherPrevItem
	// e pSonNextItem. Qualora essi siano allineati (cioe' se segmenti, appartenenti alla stessa retta,
	// se archi, appartenenti allo stesso cerchio), allora si verifica se la loro prosecuzione che li 
	// "unisce" e' all'interno dell'asola tranciata. In tal caso si inserisce tale prosecuzione.
	// Se invece non sono allineati, si verifica la possibilita' di allungarli fino a farli intersecare.
	// Se i due item cosi' determinati sono interni all'asola tranciata, essi vengono inseriti 
	// al posto della semiasola.
	// Qualora tale procedimento non abbia avuto successo, si inseriscono nel bordo uno o due segmenti 
	// paralleli agli assi X e Y che uniscano in maniera consistente pPrevItem e pNextItem.

	int nFound = 0;
	CgItem *pFirstNew = NULL;
	CgItem *pSecondNew = NULL;
	if (pFatherPrevItem->GetType() == CgItem::TYPE_ARC) {
		if (pSonNextItem->GetType() == CgItem::TYPE_ARC) {
			// In questo caso  sono sicuramente "esterni" alla semiasola.
			// Potrebbero essere due parti di uno stesso cerchio (oppure anche lo stesso item, nel caso
			// del foro della serratura.
			CgArc *pFatherArc = (CgArc *)pFatherPrevItem;
			CgArc *pSonArc = (CgArc *)pSonNextItem;
			if (IsLessEqual(Distance(pFatherArc->GetCenter(), pSonArc->GetCenter()),0.0) &&
				IsLessEqual(fabs(pFatherArc->GetRadius()-pSonArc->GetRadius()),resabs)) {
				// Archi appartenenti allo stesso cerchio.
				bool IsClockWise = true;
				if (pFatherArc->GetSignedAngle() < 0)
					IsClockWise = false;
				CgArc SubsArc(pFatherArc->GetCenter(), pFatherArc->GetEnd(), pSonArc->GetStart(), IsClockWise);
				CgBorder bTmp;
				bTmp.AddItem(&SubsArc);

				bool bGoingOut = CUtilsGeom::GoBorderOutBorder(bTmp, borObrExact);
				bTmp.Reset();
				if (!bGoingOut) {
					// Basta unire i due archi, cioe' inserire SubsArc.
					CgItem *pIns;
					int nNumDelEle = 5;
					if (pFatherArc == pSonArc) {
						CgCircle *pC = new CgCircle(pFatherArc->GetCenter(), pFatherArc->GetRadius());
						border.DeleteAllItems();
						//border.Reset();
						border.AddItem(pC);

						border.GotoFirstItem();
					}
					else {
						CgArc *pNewArc = new CgArc(pFatherArc->GetCenter(), pFatherArc->GetStart(), 
													pSonArc->GetEnd(), IsClockWise);
						pIns = (CgItem *)pNewArc;
						border.SetCurrItemPos(posPrev);
						border.GetPrevItem();
						border.ReplaceItems(nNumDelEle, pIns, true);
					}
					posArc = border.GetCurrItemPos();
					nFound = 1;
				}
			}
		}
	}
	else if (pSonNextItem->GetType() == CgItem::TYPE_LINE) {
		// Potrebbero essere due parti di una stessa retta.
		// Attenzione: possono essere sia elementi della semiasola che quelli immediatamente
		// precedente e seguente.
		CgLine *pFatherLine = (CgLine *)pFatherPrevItem;
		CgLine *pSonLine = (CgLine *)pSonNextItem;
		if (IsColinear(*pFatherLine, *pSonLine)) {
			// Segmenti appartenenti alla stessa retta: posso sostituire le semiasola con il 
			// segmento che li unisce?
			// In questo caso  sono sicuramente "esterni" alla semiasola.
			CgLine SubsLine(pFatherLine->GetEnd(), pSonLine->GetStart());
			CgBorder bTmp;
			bTmp.AddItem(&SubsLine);
			bool bGoingOut = CUtilsGeom::GoBorderOutBorder(bTmp, borObrExact);
			bTmp.Reset();
			if (!bGoingOut) {
				// Basta unire i due segmenti, cioe' inserire SubsLine.
				CgLine *pNewLine = new CgLine(pFatherLine->GetEnd(), pSonLine->GetStart());
				border.SetCurrItemPos(posPrev);
				border.ReplaceItems(3, pNewLine, true);
				posArc = border.GetCurrItemPos();
				nFound = 1;
			}
		}
	}
	if (nFound == 1) {
		// Sostituzione gia' eseguita: ho finito
		// Elimino istanze degli item dal bordo d'appoggio utilizzato.
		borObrExact.Reset();
		return true;
	}

	// Se gli item precedente e seguente all'arco di 180 gradi sono paralleli ad un asse 
	// (e visto che sono paralleli tra loro basta verificarne uno) allora mi conviene
	// sostituire il solo arco con un segmento a loro perpendicolare in maniera da 
	// lasciare piu' liberi possibile i metodi di risoluzione seguenti (e fidando
	// che eventuali punzonature inutili vengano eliminate).
	CgLine *pPrevLine = (CgLine *)pPrevItem;
	if (IsBiparallel(pPrevLine->GetDir(), X_AXIS) || IsBiparallel(pPrevLine->GetDir(), Y_AXIS)) {
		CgBorder MyBor;
		CgLine *pL = new CgLine(pPrevLine->GetEnd(), pNextItem->GetStart());
		MyBor.AddItem(pL);
		border.SetCurrItemPos(posArc);
		border.ReplaceItems(1, &MyBor, true);
		MyBor.DeleteAllItems();
		posArc = border.GetCurrItemPos();
		// Sostituzione gia' eseguita: ho finito
		// Elimino istanze degli item dal bordo d'appoggio utilizzato.
		borObrExact.Reset();
		return true;
	}

	// Attenzione: possono essere sia elementi della semiasola che quelli immediatamente
	// precedente e seguente.
	// Verifico se posso prolungare pFatherPrevItem e pSonNextItem fino a farli intersecare.
	if (pFatherPrevItem->GetType() == CgItem::TYPE_ARC) {
		CgArc *pFather = (CgArc *)pFatherPrevItem;
		if (pSonNextItem->GetType() == CgItem::TYPE_ARC) {
			// In questo caso  sono sicuramente "esterni" alla semiasola.
			CgArc *pSon = (CgArc *)pSonNextItem;
			CbPointArray aInts;
			int nNumInts = Intersect(*pFather,*pSon,aInts,false);
			if (nNumInts > 0) {
				CbPoint Pint(aInts[0]);
				for (int j=1; j<nNumInts; j++) {
					if (IsLess(Distance(aInts[j],pFather->GetEnd()),Distance(Pint,pFather->GetEnd())))
						Pint = aInts[j];
				}
				bool IsClockWise1 = true;
				if (pFather->GetSignedAngle() < 0)
					IsClockWise1 = false;
				CgArc *pFirst = new CgArc(pFather->GetCenter(), pFather->GetEnd(), Pint, IsClockWise1);
				bool IsClockWise2 = true;
				if (pSon->GetSignedAngle() < 0)
					IsClockWise2 = false;
				CgArc *pSecond = new CgArc(pSon->GetCenter(), Pint, pSon->GetStart(), IsClockWise2);
				CgBorder bTmp;
				bTmp.AddItem(pFirst);
				bTmp.AddItem(pSecond);
				bool bGoingOut = CUtilsGeom::GoBorderOutBorder(bTmp, borObrExact);
				bTmp.Reset();
				if (!bGoingOut) {
					if (pFather != pSon) {
						pSecondNew = new CgArc(pSon->GetCenter(), Pint, pSon->GetEnd(), IsClockWise2);
						pFirstNew = new CgArc(pFather->GetCenter(), pFather->GetStart(), Pint, IsClockWise1);
					}
					else {
						pFirstNew = new CgCircle(pFather->GetCenter(), pFather->GetRadius());
					}
					// Mi posiziono sul primo degli item da cancellare.
					border.SetCurrItemPos(posPrev);
					border.GetPrevItem();
					posPrev = border.GetCurrItemPos();
					nFound = -5;	// Sostituisco 5 old items con 2 new ones
				}
				pFirst->DecRef();
				pSecond->DecRef();
			}
		} else {
			CgLine *pSon = (CgLine *)pSonNextItem;
			CbPointArray aInts;
			int nNumInts = Intersect(*pFather,*pSon,aInts,false);
			if (nNumInts > 0) {
				CbPoint Pint(aInts[0]);
				for (int j=1; j<nNumInts; j++) {
					if (IsLess(Distance(aInts[j],pFather->GetEnd()),Distance(Pint,pFather->GetEnd())))
						Pint = aInts[j];
				}
				bool IsClockWise = true;
				if (pFather->GetSignedAngle() < 0)
					IsClockWise = false;
				CgArc *pFirst = new CgArc(pFather->GetCenter(), pFather->GetEnd(), Pint, IsClockWise);
				CgLine *pSecond = new CgLine(Pint, pSon->GetStart());
				CgBorder bTmp;
				bTmp.AddItem(pFirst);
				bTmp.AddItem(pSecond);
				bool bGoingOut = CUtilsGeom::GoBorderOutBorder(bTmp, borObrExact);
				bTmp.Reset();
				if (!bGoingOut) {
					pFirstNew = new CgArc(pFather->GetCenter(), pFather->GetStart(), Pint, IsClockWise);
					pSecondNew = new CgLine(Pint, pSon->GetStart());
					// Mi posiziono sul primo degli item da cancellare.
					border.SetCurrItemPos(posPrev);
					border.GetPrevItem();
					posPrev = border.GetCurrItemPos();
					nFound = -4;	// Sostituisco 4 old items con 2 new ones
				}
				pFirst->DecRef();
				pSecond->DecRef();
			}
		}
	}
	else {
		CgLine *pFather = (CgLine *)pFatherPrevItem;
		if (pSonNextItem->GetType() == CgItem::TYPE_ARC) {
			CgArc *pSon = (CgArc *)pSonNextItem;
			CbPointArray aInts;
			int nNumInts = Intersect(*pFather,*pSon,aInts,false);
			if (nNumInts > 0) {
				CbPoint Pint(aInts[0]);
				for (int j=1; j<nNumInts; j++) {
					if (IsLess(Distance(aInts[j],pFather->GetEnd()),Distance(Pint,pFather->GetEnd())))
						Pint = aInts[j];
				}
				bool IsClockWise = true;
				if (pSon->GetSignedAngle() < 0)
					IsClockWise = false;
				CgArc *pSecond = new CgArc(pSon->GetCenter(), Pint, pSon->GetStart(), IsClockWise);
				CgLine *pFirst;
				if (posPrev != posFatherPrev) {
					pFirst = new CgLine(pFather->GetEnd(), Pint);
					CgBorder bTmp;
					bTmp.AddItem(pFirst);
					bTmp.AddItem(pSecond);
					bool bGoingOut = CUtilsGeom::GoBorderOutBorder(bTmp, borObrExact);
					bTmp.Reset();
					if (!bGoingOut) {
						pSecondNew = new CgArc(pSon->GetCenter(), Pint, pSon->GetEnd(), IsClockWise);
						pFirstNew = new CgLine(pFather->GetEnd(), Pint);
						// Il primo degli item da cancellare e' posPrev.
						nFound = -4;	// Sostituisco 4 old items con 2 new ones
					}
				}
				else {
					pFirst = new CgLine(pFather->GetStart(), Pint);
					CgBorder bTmp;
					bTmp.AddItem(pFirst);
					bTmp.AddItem(pSecond);
					bool bGoingOut = CUtilsGeom::GoBorderOutBorder(bTmp, borObrExact);
					bTmp.Reset();
					if (!bGoingOut) {
						pSecondNew = new CgArc(pSon->GetCenter(), Pint, pSon->GetEnd(), IsClockWise);
						pFirstNew = new CgLine(pFather->GetStart(), Pint);
						// Il primo degli item da cancellare e' posPrev.
						nFound = -4;	// Sostituisco 4 old items con 2 new ones
					}
				}
				pFirst->DecRef();
				pSecond->DecRef();
			}
		}
		else {
			CgLine *pSon = (CgLine *)pSonNextItem;
			CbPoint Pint;
			int nNumInts = Intersect(*pFather,*pSon,Pint,false);
			if (nNumInts > 0) {
				CgLine *pFirst;
				if (posPrev != posFatherPrev)
					pFirst = new CgLine(pFather->GetEnd(), Pint);
				else
					pFirst = NULL;
				CgLine *pSecond;
				if (posNext != posSonNext)
					pSecond = new CgLine(Pint, pSon->GetStart());
				else
					pSecond = NULL;
				CgBorder bTmp;
				if (pFirst)
					bTmp.AddItem(pFirst);
				if (pSecond)
					bTmp.AddItem(pSecond);
				bool bGoingOut = true;
				if (bTmp.CountItems() != 0)
					bGoingOut = CUtilsGeom::GoBorderOutBorder(bTmp, borObrExact);
				bTmp.Reset();
				if (!bGoingOut) {
					if (pFather != pSon) {
						if (posPrev != posFatherPrev)
							pFirstNew = new CgLine(pFather->GetEnd(), Pint);
						else
							pFirstNew = new CgLine(pFather->GetStart(), Pint);
						if (posNext != posSonNext)
							pSecondNew = new CgLine(Pint, pSon->GetStart());
						else
							pSecondNew = new CgLine(Pint, pSon->GetEnd());
						}
					else {
						pFirstNew = new CgLine(pFather->GetEnd(), pSon->GetStart());
					}
					// Il primo degli item da cancellare e' posPrev.
					nFound = -3;
				}
				if (pFirst)
					pFirst->DecRef();
				if (pSecond)
					pSecond->DecRef();
			}
		}
	}

	// Elimino istanze degli item dal bordo d'appoggio utilizzato.
	borObrExact.Reset();

	if (nFound < 0) {
		// Inserisco gli items trovati al posto degli -nFound items a partire da posPrev
		CgBorder MyBor;
		MyBor.AddItem(pFirstNew);
		if (pSecondNew)
			MyBor.AddItem(pSecondNew);
		border.SetCurrItemPos(posPrev);
		border.ReplaceItems(-nFound, &MyBor, true);
		MyBor.DeleteAllItems();
		posArc = border.GetCurrItemPos();
	}
	else if (nFound == 0) {
		// Creo una spezzata che chiude la semiasola ortogonalmente ai due segmenti paralleli 
		// e passa per l'estremi del segmento piu` corto fra i due
		CbPoint p1, p2;
		CgLine *pFirstLine = NULL;
		CgLine *pSecondLine = NULL;
		if (IsSameValue(((CgLine*)pPrevItem)->GetLen(),((CgLine*)pNextItem)->GetLen())) {
			p1 = ((CgLine*)pPrevItem)->GetStart();
			p2 = ((CgLine*)pNextItem)->GetEnd();
			pFirstLine = new CgLine(p1,p2);
		}
		else if (((CgLine*)pPrevItem)->GetLen() > ((CgLine*)pNextItem)->GetLen())
		{
			p2 = ((CgLine*)pNextItem)->GetEnd();
			p1 = ((CgLine*)pPrevItem)->Perp(p2);
			pSecondLine = new CgLine(p1, p2);
			pFirstLine = new CgLine(((CgLine*)pPrevItem)->GetStart(),p1);
		}
		else
		{
			p1 = ((CgLine*)pPrevItem)->GetStart();
			p2 = ((CgLine*)pNextItem)->Perp(p1);
			pFirstLine = new CgLine(p1, p2);
			pSecondLine = new CgLine(p2,((CgLine*)pNextItem)->GetEnd());
		}


		// Sostituzione della semiasola nel bordo con la spezzata.
		border.SetCurrItemPos(posPrev);
		CgBorder MyBor;
		MyBor.AddItem(pFirstLine);
		if (pSecondLine) 
			MyBor.AddItem(pSecondLine);
		border.ReplaceItems(3, &MyBor, true);
		MyBor.DeleteAllItems();
		posArc = border.GetCurrItemPos();
	}

	return true;
}

//----------------------------------------------------------------------
//
// FUNCTION:		PunchDemiObround
//
// ARGUMENTS:	border - bordo da lavorare
//               posArc - posizione del primo arco della demiasola nel bordo
//				iToolType - tipo di utensile
//				bUseNibbling - uso utensile in roditura
//				pSavedBorder - puntatore al bordo originale, prima di eventuali modifiche.
//
// RETURN VALUE: 0: punzonatura fallita
//               1: risolti solo gli archi
//               2: demiasola completamente punzonata
//
// AUTHOR(S):    E. Bertuzzi
//
// DATE:			05/98
//
// DESCRIPTION:  Punzonatura di una demiasola di un bordo
//
//----------------------------------------------------------------------
int 
CArcsBorderWorkStrategy::PunchDemiObround(CgBorder &border, POSITION posArc, int iToolType, 
										  bool bUseNibbling, CgBorder *pSavedBorder)
{	
	CgToolObround	*pToolObr;

	border.SetCurrItemPos(posArc);
	CgArc *pArc = (CgArc *) border.GetCurrItem();
	double dRad = pArc->GetRadius();
	CbPoint c = pArc->GetCenter();

	// Prendo i dati della demiasola
	CgLine *pLineNext = (CgLine *) border.GetNextItem();
	double dLenLine = pLineNext->GetLen();
	CbAngle ang = pLineNext->GetDir().Angle();
	CbUnitVector u (ang);
	double dHeight = 2.0 * dRad;
	double dWidth = dLenLine + dHeight;

	// Metodo chiamato dopo aver verificato la presenza di una demiasola: devo pero'
	// verificare se essa e' punzonabile senza "rovinare" il contorno (cioe' se l'asola
	// esce dal contorno stesso).
	CgBorder borCheck;
	CbVector vToStart(dRad, pArc->GetEndAngle() - M_PI);
	CgArc arc1(c, dRad, vToStart, M_PI);
	borCheck.AddItem(&arc1);
	borCheck.AddItem(pLineNext);
	CgArc *pArcNext = (CgArc *)border.GetNextItem();
	CgArc arc2(pArcNext->GetCenter(), dRad, pArcNext->GetStartVect(), M_PI);
	borCheck.AddItem(&arc2);
	CgLine ll(arc2.GetEnd(), arc1.GetStart());
	borCheck.AddItem(&ll);
	bool bIsOut = CUtilsGeom::GoBorderOutBorder(borCheck, *pSavedBorder);
	borCheck.Reset();
	if (bIsOut)
		return 0;

	// gestione approssimazione percentuale utensili
	double dMaxCirAppW = GetParameters().dMaxCirApp;
	if(GetParameters().bUsePercMaxCirApp)
		dMaxCirAppW = dWidth * GetParameters().dPercMaxCirApp / 100.0;
	double dMaxCirAppH = GetParameters().dMaxCirApp;
	if(GetParameters().bUsePercMaxCirApp)
		dMaxCirAppH = dHeight * GetParameters().dPercMaxCirApp / 100.0;

	double dMinCirAppW = GetParameters().dMinCirApp;
	if(GetParameters().bUsePercMinCirApp)
		dMinCirAppW = dWidth * GetParameters().dPercMinCirApp / 100.0;
	double dMinCirAppH = GetParameters().dMinCirApp;
	if(GetParameters().bUsePercMinCirApp)
		dMinCirAppH = dHeight * GetParameters().dPercMinCirApp / 100.0;

	// Controllo se c'e` il punzone ad asola esatto
	CgToolArray aObrTools;
	int iNumOfTools = GET_TOOL_DB.FindObround(aObrTools, iToolType, 
										 CbCond(dWidth, dMinCirAppW, dMaxCirAppW), 
										 CbCond(dHeight, dMinCirAppH, dMaxCirAppH), 
										 CbCond(ang.Get(), 0.0, 0.0, resnor), false, false);
	// Se c'e`, il primo e` il migliore
	for (int iP=0; iP<iNumOfTools; iP++)
	{
		pToolObr = (CgToolObround *)aObrTools[iP];
		CbPoint Pcenter(pArc->GetCenter() + CbVector(dLenLine*0.5,ang));
		// Verifico se utensile e' utilizzabile in questa posizione.
		if (!CUtilsTools::IsToolUsableInPoint(GET_TOOL_DB, pToolObr, Pcenter))
			continue;
		
		// Programmo la punzonatura
		CmXyp *pXyp = new CmXyp(&GETPUNCHPROGRAM);
		pXyp->SetRefPoint(Pcenter);
		pXyp->SetUsedTool(pToolObr);
		// Se e` rotante devo programmare l'angolo corretto
		if (pToolObr->IsRotating())
			pXyp->SetToolAngle(ang - pToolObr->GetTotalRotation());
		GETPUNCHPROGRAM.SAVE_PUNCH_OPERATION(pXyp, &border);

		return 2;
	}

	// Se non c'e` l'utensile esatto, controllo se c'e` un'asola avente la stessa altezza ma base 
	// piu` piccola, a meno che il tipo di utensile richiesto non sia incompatibile con l'uso
	// in roditura, nel qual caso esco subito. Se l'utensile c'e`, lo uso senza condizioni se
	// il numero di colpi necessari per coprire l'intera demiasola e` minore o uguale a 3. Se
	// il numero di colpi e` superiore a 3, confronto tale numero con quello che si otterrebbe
	// distruggendo i due archi con l'utensile ad asola e la parte rimanente con utensili 
	// rettangolari. Se e` migliore la soluzione con l'asola, programmo la STR, altrimenti
	// non faccio nulla: i due archi e le aree rettangolari verranno poi gestiti singolarmente.
	if (!bUseNibbling)
		return 0;

	iNumOfTools = GET_TOOL_DB.FindObround(aObrTools, 
									 iToolType, 
									 CbCond(dWidth, dWidth, 0.0), 
									 CbCond(dHeight, dMinCirAppH, dMaxCirAppH), 
									 CbCond(ang.Get(), 0.0, 0.0, resnor), 
									 true, 
									 false);
	// Se c'e`, il primo e` il migliore (a meno che sovrapposizione sia troppo stretta).
	for (int iTool=0; iTool < iNumOfTools; iTool++)
	{
		pToolObr = (CgToolObround *)aObrTools[iTool];

		// Lunghezza del lato rettilineo del tool
		double dSegmTool = pToolObr->GetWidth() - pToolObr->GetHeight();

		// Calcolo il numero di punzonature, riducendo della sovrapposizione interna 
		// la lunghezza dell'utensile in modo da essere conservativo
		int iNumOfPunchObr = (int) ceil((dLenLine) / (dSegmTool - GetParameters().dIntOverlap));

		if (iNumOfPunchObr > 3) {
			// Controllo se c'e` un utensile rettangolare che possa distruggere la parte rettilinea
			// della semiasola
			CgToolArray	aRectTools;
			CgToolRect	*pToolRect;
			int			iNumOfRecTools,
						iNumOfPunchRec = 0;
			double		dWidthTool, dHeightTool;

			iNumOfRecTools = GET_TOOL_DB.FindRect(aRectTools, iToolType, CbCond(dLenLine, dLenLine, 0.0), 
						CbCond(dRad, 0.0, COND_IGNORE), CbCond(ang.Get(), 0.0, 0.0, resnor), true, false);
			for (int i=0; i<iNumOfRecTools; i++) {
				pToolRect = (CgToolRect *)aRectTools[i];

				dWidthTool = pToolRect->GetWidth();
				dHeightTool = pToolRect->GetHeight();

				// Ricerca del lato del rettangolo su cui fare i controlli
				double dSideOk;
				double dOtherSide;
				if (IsGreaterEqual(dWidthTool, dRad) && IsGreaterEqual(dHeightTool, dRad)) {
					dSideOk = mymax(dWidthTool, dHeightTool);
					dOtherSide = mymin(dWidthTool, dHeightTool);
				}
				else if (IsLess(dHeightTool, dRad)) {
					dSideOk = dHeightTool;
					dOtherSide = dWidthTool;
				}
				else {
					dSideOk = dWidthTool;
					dOtherSide = dHeightTool;
				}

				// Verifico se si puo' costruire REC di altezza dOtherSide sul segmento tra i due
				// archi (altrimenti utensile non utilizzabile.
				CgBorder MyBor;
				CgLine Lin1(*pLineNext);
				CgLine Lin2(pLineNext->GetEnd(), pLineNext->GetEnd()+CbVector(dOtherSide,ang+M_PI_2));
				CgLine Lin3(Lin2.GetEnd(), Lin2.GetEnd()-CbVector(dLenLine, ang));
				CgLine Lin4(Lin3.GetEnd(),Lin1.GetStart());
				MyBor.AddItem(&Lin1);
				MyBor.AddItem(&Lin2);
				MyBor.AddItem(&Lin3);
				MyBor.AddItem(&Lin4);
				bool bGoingOut = CUtilsGeom::GoBorderOutBorder(MyBor, *pSavedBorder);
				MyBor.Reset();
				if (bGoingOut) 
					continue;

				// Posso utilizzare l'utensile i-esimo.

				iNumOfPunchRec = (int) ceil((dLenLine) / (dSideOk - GetParameters().dIntOverlap));

				// Ora confronto il numero di punzonature con quello ottenuto con il punzone ad
				// asola (quest'ultimo diminuito di due in quanto include anche i due colpi
				// estremi che distruggono i due archi). Se la soluzione migliore e` con il
				// rettangolare, controllo se c'e` l'utensile circolare che ha lo stesso raggio
				// dei due archi. Se c'e` non faccio nulla ed esco (perche` l'area sara` risolta
				// dopo), se non c'e` mi limito a fare i due colpi con l'utensile ad asola per 
				// distruggere gli archi e poi esco.
				if (iNumOfPunchRec < iNumOfPunchObr)
				{
					// gestione percentuale approssimazione utensili
					double dMaxCirAppR = GetParameters().dMaxCirApp;
					if(GetParameters().bUsePercMaxCirApp)
						dMaxCirAppR = dRad * GetParameters().dPercMaxCirApp / 100.0;
					double dMinCirAppR = GetParameters().dMinCirApp;
					if(GetParameters().bUsePercMinCirApp)
						dMinCirAppR = dRad * GetParameters().dPercMinCirApp / 100.0;

					CgToolArray aCircTools;
					if (GET_TOOL_DB.FindCirc(aCircTools, iToolType, 
							CbCond(dRad, dMinCirAppR, dMaxCirAppR), false) != 0)
						return 0;
					else
					{
						// Centro della prima punzonatura
						CbPoint p1 = c + u * (pToolObr->GetWidth() / 2.0 - dHeight / 2.0);

						// Programmo il primo colpo
						CmXyp *pXyp1 = new CmXyp(&GETPUNCHPROGRAM);
						pXyp1->SetRefPoint(p1);
						pXyp1->SetUsedTool(pToolObr);
						// Se e` rotante devo programmare l'angolo corretto
						if (pToolObr->IsRotating())
							pXyp1->SetToolAngle(ang - pToolObr->GetTotalRotation());
						GETPUNCHPROGRAM.SAVE_PUNCH_OPERATION(pXyp1, &border);

						// Centro della seconda punzonatura
						CbPoint p2 = p1 + u * (dWidth - pToolObr->GetWidth());

						// Programmo il secondo colpo
						CmXyp *pXyp2 = new CmXyp(&GETPUNCHPROGRAM);
						pXyp2->SetRefPoint(p2);
						pXyp2->SetUsedTool(pToolObr);
						// Se e` rotante devo programmare l'angolo corretto
						if (pToolObr->IsRotating())
							pXyp2->SetToolAngle(ang - pToolObr->GetTotalRotation());
						GETPUNCHPROGRAM.SAVE_PUNCH_OPERATION(pXyp2, &border);

						return 1;
					}
				}
			}
		}

		// A questo punto la soluzione migliore e` con l'utensile ad asola: programmo la STR.
		double dToolW = pToolObr->GetWidth() - pToolObr->GetHeight();
		double dInterAxis = 0.0;

		// M Beninca 2/7/2001
		// l'interasse va calcola tenendo conto che se ho usato un utensile non preciso sto
		// partendo sfasato nella roditura della linea di una lunghezza pari alla differenza
		// tra il raggio dell'arco da risolvere - il raggio dell'utensile... correzione che
		// va moltiplicata per 2 per tener conto che lo stasso errore si commette nel lato opposto
		// della roditura!
		if(iNumOfPunchObr > 1)
			dInterAxis = (dLenLine + (2.0 * dRad - pToolObr->GetHeight()) - dToolW) / (iNumOfPunchObr - 1);

		//if (IsLess(dInterAxis, GetParameters().dMinScrapRec))
		// Vedi commento sopra (richiesta di G. Rossetto...)
		if (IsLess(dInterAxis, GetParameters().dIntOverlap))
			continue;

		// Centro della prima punzonatura
		CbPoint p1 = c + u * (pToolObr->GetWidth() / 2.0 - dHeight / 2.0);

		// Programmo la punzonatura
		CmStr *pStr = new CmStr(&GETPUNCHPROGRAM);
		pStr->SetRefPoint(p1);
		pStr->SetUsedTool(pToolObr);
		// Se e` rotante devo programmare l'angolo corretto
		if (pToolObr->IsRotating())
			pStr->SetToolAngle(ang - pToolObr->GetTotalRotation());
		pStr->SetSlope(ang);
		pStr->SetPunchStep(dInterAxis);
		pStr->SetPunchNumber(iNumOfPunchObr);
		GETPUNCHPROGRAM.SAVE_PUNCH_OPERATION(pStr, &border);
		
		return 2;
	}

	return 0;
}

//----------------------------------------------------------------------
//
// FUNCTION:	BananaMng
//
// ARGUMENTS:	border - bordo
//				iToolType - tipo di utensile
//				bUseNibbling - uso utensile in roditura
//
// RETURN VALUE: 1 se il bordo e` completamente risolto, 0 altrimenti, -1 aborted
//
// AUTHOR(S):    E. Bertuzzi
//
// DATE:			05/98
//
// DESCRIPTION:  Riconosce, punzona e sostituisce profili "a banana".
//
//----------------------------------------------------------------------
int 
CArcsBorderWorkStrategy::BananaMng(CAMBORDER &border, int iToolType, bool bUseNibbling)
{

	// Il bordo e` a banana se ha 4 items e se sono archi
	if (border.IsBananaShape())
	{
		// Chiamata alla routine che cerca di punzonare la banana
		return PunchBanana(border, iToolType, bUseNibbling);
	}

	return 0;
}

//----------------------------------------------------------------------
//
// FUNCTION:		PunchBanana
//
// ARGUMENTS:	border - bordo a banana
//				iToolType - tipo di utensile
//				bUseNibbling - uso utensile in roditura
//
// RETURN VALUE: 1 se la punzonatura e` completata, 0 altrimenti, -1 aborted
//
// AUTHOR(S):    E. Bertuzzi
//
// DATE:			05/98
//
// DESCRIPTION:  Punzonatura di un bordo a banana
//
//----------------------------------------------------------------------
int 
CArcsBorderWorkStrategy::PunchBanana(CAMBORDER &border, int iToolType, bool bUseNibbling)
{
	double			dRad1,
					dRad2,
					dWidthBanana;
	CgToolCirc		*pToolCirc;
	CgToolSpecial	*pToolSpecial;
	CgArc			*pFirstArc;
	CbPoint			c;
	CbAngle			angBanana;
	CbVector		vecCen;
	POSITION		posFirstArc;
	bool			bDone = false;


	// Ricavo i dati del bordo a banana: centro dei due archi che hanno il centro in comune, il
	// loro angolo, i loro raggi e la posizione, nel bordo, del primo arco di 180 gradi, vale a 
	// dire quell'arco da cui partirebbe la roditura nel caso la banana venisse distrutta con un 
	// utensile circolare avente il diametro pari alla larghezza della banana
	GetBananaData(border, c, angBanana, dRad1, dRad2, posFirstArc, vecCen);
	dWidthBanana = dRad2 - dRad1;
	border.SetCurrItemPos(posFirstArc);
	pFirstArc = (CgArc *)border.GetCurrItem();
	CbPoint c1 = pFirstArc->GetCenter();

	// Controllo se c'e` il punzone speciale che abbia la stessa larghezza della banana e 
	// ampiezza angolare minore o uguale a quella della banana. La condizione CbCond e` sul numero
	// di bordi che compongono l'utensile.
	CgToolArray aSpecTools;
	CgImage *pImageTool;
	CgBorder *pBorTool;
	bool bAbort = false;
	int iNumOfTools = GET_TOOL_DB.FindSpecial(aSpecTools, iToolType, CbCond(1.0, 0.0, 0.0), false, false);
	int i;
	for (i=0; i<iNumOfTools && !bAbort; i++)
	{
		if (WAIT_THREAD->IsStopped()) {
			bAbort = true;
			continue;
		}
		pToolSpecial = (CgToolSpecial *)aSpecTools[i];

		pImageTool = pToolSpecial->GetImage();
		if (pImageTool == NULL) 
			continue;
		pBorTool = pImageTool->GetBorder(0);

		// Se l'utensile ha forma a banana, controllo se e` compatibile col bordo
		// a banana da distruggere
		if (pBorTool->IsBananaShape())
		{
			double		dRadTool1, dRadTool2;
			CbAngle		angTool;
			CbPoint		cenTool, p;
			POSITION	posFirstArcTool;
			CbVector	vecCenTool;

			// Prendo i dati del bordo associato al tool e li confronto col
			// bordo da distruggere. L'utensile puo` avere una ampiezza angolare piu` piccola
			// di quella del bordo a banana
			GetBananaData(*pBorTool, cenTool, angTool, dRadTool1, dRadTool2, posFirstArcTool, vecCenTool);
			pBorTool->SetCurrItemPos(posFirstArcTool);
			//CgArc *pFirstArcTool = (CgArc *)pBorTool->GetCurrItem();
			
			if (IsSameValue(dRad1, dRadTool1) && IsSameValue(dRad2, dRadTool2))
			{
				// Per ottenere la traslazione corretta dell'utensile, prendo
				// un vettore che va dall'origine al centro dei due archi che hanno il 
				// centro in comune (il centro dell'innagine del punzone e` sempre l'origine)
				CbVector vecTransl = cenTool - CbPoint(0.0, 0.0);

				// Continuo nei seguenti due casi
				// - l'ampiezza angolare del tool e` uguale a quella del bordo e
				//   l'utensile ha la stessa inclinazione o e` rotante
				// - l'ampiezza angolare del tool e` piu piccola di quella del bordo
				//   e l'utensile e` rotante ed e` di tipo compatibile con la roditura
				if (IsSameAngle(angTool,angBanana,0.5*M_PI/180.0))
				{					
					// Se le inclinazioni dei due vettori sono uguali o se l'utensile e`
					// rotante, la punzonatura si puo` fare.
					// Angolo di rotazione del foro rispetto all'utensile
					CbAngle angRot = vecCen.Angle() - vecCenTool.Angle();

					if (angRot == CbAngle(0.0) || pToolSpecial->IsRotating())
					{
						// Posizione dell'utensile per fare la punzonatura (nel caso non
						// rotante)
						p = c - vecTransl;

						if (pToolSpecial->IsRotating())
						{
							// Angolo di rotazione del foro rispetto all'utensile
							angRot = vecCen.Angle() - vecCenTool.Angle();

							// Pensando all'utensile ruotato dell'angolo di rotazione, prendo
							// un vettore che va dall'origine al centro dei due archi che hanno il
							// centro in comune
							CbVector vecTranslMod (vecTransl.Len(), vecTransl.Angle() + angRot);

							// Posizione dell'utensile per fare la punzonatura
							p = c - vecTranslMod;
						}
						bDone = true;
					}
					if (bDone)
					{
						// Verifico se utensile e' utilizzabile in questa posizione.
						if (!CUtilsTools::IsToolUsableInPoint(GET_TOOL_DB, pToolSpecial, p))
							continue;

						CmXyp *pXyp = new CmXyp(&GETPUNCHPROGRAM);
						pXyp->SetRefPoint(p);
						pXyp->SetUsedTool(pToolSpecial);
						pXyp->SetToolAngle(angRot);
						GETPUNCHPROGRAM.SAVE_PUNCH_OPERATION(pXyp, &border);
					}
				}
				else if ((angTool < angBanana) && pToolSpecial->IsRotating() && bUseNibbling && 
						 iToolType == CgTool::TYPE_PUNCH)
				{
					// Calcolo del centro del primo colpo della POL rispetto al centro degli
					// archi a banana aventi centro comune
					CbAngle ang1 = vecCen.Angle() - angBanana / 2.0 + angTool / 2.0;
					
					// Angolo di rotazione del primo colpo rispetto alla posizione base dell'utensile
					CbAngle angRot = ang1 - vecCenTool.Angle();

					// Pensando all'utensile ruotato (nell'origine) dell'angolo di rotazione, 
					// prendo un vettore che va dall'origine al centro dei due archi che hanno il
					// centro in comune
					CbVector vecTranslMod (vecTransl.Len(), vecTransl.Angle() + angRot);
					
					// Numero di punzonature necessarie
					int iNumOfPunch = (int) ceil(angBanana.Get() / angTool.Get());

					// Passo angolare 
					CbAngle angPitch ((angBanana - angTool).Get() / (iNumOfPunch - 1));

					// costruzione della macro
					CmPol *pPol = new CmPol(&GETPUNCHPROGRAM);
					pPol->SetRefPoint(c);
					pPol->SetUsedTool(pToolSpecial);
					pPol->SetToolAngle(angRot);
					pPol->SetSlope((-vecTranslMod).Angle());
					pPol->SetAngleBetween(angPitch);
					pPol->SetWidth(vecTranslMod.Len());
					pPol->SetPunchNumber(iNumOfPunch);
					pPol->SetCEHStatus(true);
					GETPUNCHPROGRAM.SAVE_PUNCH_OPERATION(pPol, &border);
					
					bDone = true;
				}
			}
		}

		pImageTool->DeleteAllItems();
		delete pImageTool;

		if (bAbort)
			return -1;
		if (bDone)
			return 1;
	}

	if (bAbort)
		return -1;

	// Se non c'e` l'utensile speciale che va bene, se il tipo dell'utensile e` compatibile 
	// con l'utilizzo in roditura controllo se c'e` l'utensile circolare avente il diametro 
	// pari alla larghezza del profilo a banana.
	if (!bUseNibbling || iToolType != CgTool::TYPE_PUNCH)
		return 1;

	// gestione approssimazione utensili percentuale
	double dMaxCirAppB = GetParameters().dMaxCirApp;
	if(GetParameters().bUsePercMaxCirApp)
		dMaxCirAppB = dWidthBanana / 2.0 * GetParameters().dPercMaxCirApp / 100.0;
	double dMinCirAppB = GetParameters().dMinCirApp;
	if(GetParameters().bUsePercMinCirApp)
		dMinCirAppB = dWidthBanana / 2.0 * GetParameters().dPercMinCirApp / 100.0;
	
	CgToolArray aCirTools;
	iNumOfTools = GET_TOOL_DB.FindCirc(aCirTools, iToolType, CbCond(dWidthBanana / 2.0, dMinCirAppB, dMaxCirAppB), false);
	// Se c'e`, il primo e` il migliore
	if (iNumOfTools != 0)
	{
		double	dRoughness;

		pToolCirc = (CgToolCirc *)aCirTools[0];

		// L'asperita` richiesta puo` essere espressa in termini assoluti o percentuali 
		// sul raggio dell'arco. Come raggio prendo la media fra dRad1 e dRad2
		if (GetParameters().bPercRoughNIB)
			dRoughness = ((dRad1 + dRad2) / 2.0) * GetParameters().dPercRoughNIB / 100.0;
		else
			dRoughness = GetParameters().dRoughNIB;

		// programmo la punzonatura
		CmRic *pRic = new CmRic(&GETPUNCHPROGRAM);
		pRic->SetRefPoint(c);
		pRic->SetUsedTool(pToolCirc);
		pRic->SetSlope(CbVector(c1 - c).Angle());
		pRic->SetNibAngle(angBanana);
		pRic->SetWidth(dRad2);
		pRic->SetRoughness(dRoughness);
		GETPUNCHPROGRAM.SAVE_PUNCH_OPERATION(pRic, &border);
	
		return 1;
	}

	if (WAIT_THREAD->IsStopped()) 
		return -1;
	
	// Se non c'e` l'utensile circolare avente il diametro pari alla larghezza del profilo a banana,
	// imposto una roditura per ognuna delle due semicirconferenze e poi una roditura
	// della parte compresa fra i due archi concentrici.
	// La routine che punzona un arco vuole come parametro anche il bordo modificato sostituendo
	// l'arco con la spezzata: come spezzata prendo la corda dell'arco, per ognuna delle due
	// semicirconferenze

	// Copia del bordo a banana
	CAMBORDER borOrig (border);

	// Bordi delle spezzate
	CgBorder bor1, bor2;

	// Allineo i puntatori agli item dei bordi
	CgArc *pArcInOrig = (CgArc *)borOrig.GetFirstItem();
	POSITION posArcInOrig;
	for (i = 0; i < border.CountItems(); i++)
	{
		if (pArcInOrig->GetCenter() == pFirstArc->GetCenter())
		{
			posArcInOrig = borOrig.GetCurrItemPos();
			break;
		}
		pArcInOrig = (CgArc *)borOrig.GetNextItem();
	}

	// Sostituisco la prima semicirconferenza con la corda
	CgLine *pLine1 = new CgLine(pArcInOrig->GetStart(), pArcInOrig->GetEnd());
	bor1.AddItem(pLine1);
	border.SetCurrItemPos(posFirstArc);
	border.ReplaceItem(pLine1);

	// Punzonatura della prima semicirconferenza: la PunchArc ritorna anche uno statement array
	// (che qui non utilizzo)
	PUNCH_INSTRUCTION_LIST lWIArc;
	POSITION posLastSubs = border.GetCurrItemPos();
	int iRetVal = PunchArc(border, borOrig, bor1, posArcInOrig, posLastSubs, pArcInOrig->GetRadius(), 
							iToolType, bUseNibbling, lWIArc);

	if (iRetVal < 0)
		return -1;

	// Sostituisco la seconda semicirconferenza con la corda
	borOrig.SetCurrItemPos(posArcInOrig);
	borOrig.GetNextItem();
	pArcInOrig = (CgArc *)borOrig.GetNextItem();
	posArcInOrig = borOrig.GetCurrItemPos();
	CgLine *pLine2 = new CgLine(pArcInOrig->GetStart(), pArcInOrig->GetEnd());
	bor2.AddItem(pLine2);
	// Devo posizionare border sull'item coincidente con pArcInOrig
	CgArc *pDum = (CgArc *)border.GetNextItem();
	while (true) {
		if (pDum->GetCenter() == pArcInOrig->GetCenter()) {
			posFirstArc = border.GetCurrItemPos();
			break;
		} else {
			pDum = (CgArc *)border.GetNextItem();
		}
	}
	border.SetCurrItemPos(posFirstArc);		// Dovrebbe essere inutile ma meglio farlo.
	border.ReplaceItem(pLine2);

	// Punzonatura della seconda semicirconferenza
	double dRadSemiCircle = pArcInOrig->GetRadius();
	posLastSubs = border.GetCurrItemPos();
	iRetVal = PunchArc(border, borOrig, bor2, posArcInOrig, posLastSubs, dRadSemiCircle, 
					   iToolType, bUseNibbling, lWIArc);

	if (iRetVal < 0)
		return -1;

	// Aggancio le istruzioni individuate
	GETPUNCHPROGRAM.SAVE_PUNCH_OPERATIONS(&lWIArc, &border);
	lWIArc.RemoveAll();
	
	// Elimino il bordo copia che avevo creato
	borOrig.DeleteAllItems();
	// ... e quelli temporanei.
	bor2.DeleteAllItems();
	bor1.DeleteAllItems();

	// Cerco ora il piu grande utensile circolare che possa fare la roditura fra i due archi
	// concentrici
	iNumOfTools = GET_TOOL_DB.FindCirc(aCirTools, iToolType, CbCond(dWidthBanana / 2.0, dWidthBanana / 2.0 - GetParameters().dMinRadNIB, 0.0), true);
	// Se c'e`, il primo e` il migliore
	if (iNumOfTools != 0)
	{
		double	dRoughness;

		pToolCirc = (CgToolCirc *)aCirTools[0];
		double dRadTool = pToolCirc->GetRadius();

		// L'asperita` richiesta puo` essere espressa in termini assoluti o percentuali 
		// sul raggio dell'arco. Come raggio prendo quello delle semicirconferenze
		if (GetParameters().bPercRoughNIB)
			dRoughness = dRadSemiCircle * GetParameters().dPercRoughNIB / 100.0;
		else
			dRoughness = GetParameters().dRoughNIB;
		
		// programmo la roditura per la parte interna dell'arco piu` grande
		CmRic *pRic = new CmRic(&GETPUNCHPROGRAM);
		pRic->SetRefPoint(c);
		pRic->SetUsedTool(pToolCirc);
		pRic->SetSlope(CbVector(c1 - c).Angle());
		pRic->SetNibAngle(angBanana);
		pRic->SetWidth(dRad2);
		pRic->SetRoughness(dRoughness);
		GETPUNCHPROGRAM.SAVE_PUNCH_OPERATION(pRic, &border);

		// Per la sgrossatura intermedia calcolo l'asperita` ottima per avere
		// il minimo numero di colpi
		double dRadRed = dRad2 - 2.0 * dRadTool + dRoughness;
		double dWidthBananaRed = dWidthBanana - 4.0 * dRadTool + 2.0 * dRoughness;
		// Se il valore appena calcolato e` positivo si dovra` fare la sgrossatura,
		// altrimenti sono sufficienti i due giri di roditura
		if (dWidthBananaRed > 0)
		{
			double dRoughSgr = CUtilsTools::BestRoughSgr(dWidthBananaRed, dRadRed, pToolCirc, angBanana);

			// In base al valore di asperita`, calcolo il numero di giri che servono
			// per fare la sgrossatura completa
			int iNumOfNibSgr = (int) ceil(dWidthBananaRed / (2.0 * dRadTool - dRoughSgr));
			dRadRed += dRoughSgr - dRoughness;
			double dDiff = iNumOfNibSgr*(2.0*(dRadTool - dRoughSgr)) - dWidthBananaRed;
			if (IsGreater(dDiff, 0.0)) {
				// Cerco di distanziare uniformemente i colpi di distruzione interni
				dDiff /= (iNumOfNibSgr+1);
				dRadRed += dDiff;
			} else {
				dDiff = 0.0;
			}

			for (int k = 0; k < iNumOfNibSgr; k++)
			{
				pRic = new CmRic(&GETPUNCHPROGRAM);
				pRic->SetRefPoint(c);
				pRic->SetUsedTool(pToolCirc);
				pRic->SetSlope(CbVector(c1 - c).Angle());
				pRic->SetNibAngle(angBanana);
				pRic->SetWidth(dRadRed);
				pRic->SetRoughness(dRoughSgr);
				GETPUNCHPROGRAM.SAVE_PUNCH_OPERATION(pRic, &border);

				dRadRed = dRadRed - 2.0*dRadTool + dRoughSgr + dDiff;									
			}
		}
		
		// Programmo la roditura per la parte esterna dell'arco piu` piccolo
		pRic = new CmRic(&GETPUNCHPROGRAM);
		pRic->SetRefPoint(c);
		pRic->SetUsedTool(pToolCirc);
		pRic->SetSlope(CbVector(c1 - c).Angle());
		pRic->SetNibAngle(angBanana);
		pRic->SetWidth(dRad1 + 2.0 * dRadTool);
		pRic->SetRoughness(dRoughness);
		GETPUNCHPROGRAM.SAVE_PUNCH_OPERATION(pRic, &border);

		return 1;
	}

	return 0;
}

//----------------------------------------------------------------------
//
// FUNCTION:	GetBananaData
//
// ARGUMENTS:	border - bordo a banana
//
// RETURN VALUE: center - centro dei due archi che hanno il centro in comune
//               ang - angolo dei due archi che hanno il centro in comune
//				 dRad1 - il minore dei raggi dei due archi che hanno il centro in comune
//				 dRad2 - il maggiore dei raggi dei due archi che hanno il centro in comune
//               posFirstArc - posizione nel bordo del primo arco di 180 gradi, vale a dire 
//							  quell'arco da cui partirebbe la roditura nel caso la banana 
//							  venisse distrutta con un utensile circolare avente il diametro                              
//							  pari alla larghezza della banana
//               vecCenter - vettore che va dal centro dei due archi che hanno il centro il 
//                           comune al centro della banana
//
// AUTHOR(S):    E. Bertuzzi
//
// DATE:			06/98
//
// DESCRIPTION:  Dato un bordo a banana, restituisce i dati relativi
//
//----------------------------------------------------------------------
void 
CArcsBorderWorkStrategy::GetBananaData(CgBorder &border, CbPoint &center, CbAngle &ang, double &dRad1, double &dRad2, POSITION &posFirstArc, CbVector &vecCenter)
{

	CbAngle		angMid;

	CgArc		*pArc1	= (CgArc *)border.GetFirstItem();
	POSITION	pos1	= border.GetCurrItemPos();
	CgArc		*pArc2	= (CgArc *)border.GetNextItem();
	POSITION	pos2	= border.GetCurrItemPos();
	CgArc		*pArc3	= (CgArc *)border.GetNextItem();
	POSITION	pos3	= border.GetCurrItemPos();
	CgArc		*pArc4	= (CgArc *)border.GetNextItem();
	POSITION	pos4	= border.GetCurrItemPos();

	// Trovo il centro dei due archi che hanno il centro in comune, i loro raggi e il
	// centro del primo arco di 180 gradi, vale a dire quell'arco da cui partirebbe
	// la roditura nel caso la banana venisse distrutta con un utensile circolare
	// avente il diametro pari alla larghezza della banana
	if (IsSamePoint(pArc1->GetCenter(), pArc3->GetCenter()))
	{
		center = pArc1->GetCenter();
		ang = pArc1->GetAngle();
		if (pArc1->GetRadius() > pArc3->GetRadius())
		{
			angMid = pArc1->GetStartAngle() + (pArc1->GetEndAngle() - pArc1->GetStartAngle()) / 2.0;
			dRad1 = pArc3->GetRadius();
			dRad2 = pArc1->GetRadius();
			posFirstArc = pos4;
		}
		else
		{
			angMid = pArc3->GetStartAngle() + (pArc3->GetEndAngle() - pArc3->GetStartAngle()) / 2.0;
			dRad1 = pArc1->GetRadius();
			dRad2 = pArc3->GetRadius();
			posFirstArc = pos2;
		}
		vecCenter = CbVector((pArc1->GetRadius() + pArc3->GetRadius()) / 2.0, angMid);
	}
	else
	{
		center = pArc2->GetCenter();
		ang = pArc2->GetAngle();
		if (pArc2->GetRadius() > pArc4->GetRadius())
		{
			angMid = pArc2->GetStartAngle() + (pArc2->GetEndAngle() - pArc2->GetStartAngle()) / 2.0;
			dRad1 = pArc4->GetRadius();
			dRad2 = pArc2->GetRadius();
			posFirstArc = pos1;
		}
		else
		{
			angMid = pArc4->GetStartAngle() + (pArc4->GetEndAngle() - pArc4->GetStartAngle()) / 2.0;
			dRad1 = pArc2->GetRadius();
			dRad2 = pArc4->GetRadius();
			posFirstArc = pos3;
		}
		vecCenter = CbVector((pArc2->GetRadius() + pArc4->GetRadius()) / 2.0, angMid);
	}

	return;

}


//----------------------------------------------------------------------
//
// FUNCTION:	RadiusRectMng
//
// ARGUMENTS:	border - bordo
//				iToolType - tipo di utensile
//				bUseNibbling - uso utensile in roditura
//				pSavedBorder - puntatore al bordo originale, prima di eventuali modifiche.
//
// RETURN VALUE: 1 se il bordo e` completamente risolto, 0 altrimenti
//               (false anche se il bordo e` parzialmente risolto)
//				-1 aborted
//
// AUTHOR(S):   N. Tognon
//
// DATE:		10/2010
//
// DESCRIPTION:  Riconosce, punzona e sostituisce profili rettangolari smussati (completi, semi o demi)
//
//----------------------------------------------------------------------
int 
CArcsBorderWorkStrategy::RadiusRectMng(CAMBORDER *pborder, int iToolType, bool bUseNibbling, CgBorder *pSavedBorder)
{

	CbPoint			p1, p2, p;
	CbUnitVector	unitVectPrev,
					unitVectNext;
	CgItem			*pCurItem,
					*pPrevItem,
					*pNextItem;
	POSITION		pos;

	bool bBorderWasModified = true;
	bool bAbort = false;
	while (bBorderWasModified) {
		// Devo eseguire piu' loop in quanto ci sono dei casi (per quanto rari) in cui 
		// le sostituzioni eseguite dai moduli chiamati formano dei "rettangolai smussati" da considerare.
		// Queste nuove figure possono contenere degli item collineari, quindi eseguo la
		// JoinAlignedItems.
		bBorderWasModified = false;
		pborder->JoinAlignedItems();

		// Loop sugli item del bordo

		pCurItem = pborder->GetFirstItem();

		for (int i=0; i<pborder->CountItems() && !bAbort; i++)
		{
			if (WAIT_THREAD->IsStopped()) {
				bAbort = true;
				continue;
			}
			int iNumOfItems = pborder->CountItems();

			if (pCurItem->GetType() == CgItem::TYPE_ARC)
			{
				CgArc *pArc = (CgArc*) pCurItem;
				pos = pborder->GetCurrItemPos();

				// Prendo l'item precedente e seguente e le loro posizioni
				pPrevItem = pborder->GetPrevItem();
				POSITION posPrev = pborder->GetCurrItemPos();
				pborder->GetNextItem();
				pNextItem = pborder->GetNextItem();
				POSITION posNext = pborder->GetCurrItemPos();

				// Condizione necessaria per avere un rettangolo smussato 
				// e` che l'item precedente e seguente siano segmenti e siano tangenti all'arco
				bool bArcIsSolved = false;

#ifdef COMPILING_CAM_COMBI_DLL
				// La depoligonalizzazione non garantisce la tangenza delle due rette con l'arco
				// viene aumentata la risoluzione in base all'errore ammesso sulla corda
				double bktResNor = resnor;
				resnor = acos((pArc->GetRadius() - resabs)/pArc->GetRadius());
#endif
				if (pPrevItem->GetType() == CgItem::TYPE_LINE &&
					pNextItem->GetType() == CgItem::TYPE_LINE &&
					((CgLine*)pPrevItem)->GetAngle() == (pArc->GetStartTanVect()).Angle() &&
					((CgLine*)pNextItem)->GetAngle() == (pArc->GetEndTanVect()).Angle())
				{

					if (pArc->GetSignedAngle() == CbSignedAngle(M_PI_2))
					{
						// Il bordo e` un rettangolo smussato?
						if (pborder->IsRadiussedRectangle()) {
							double dTmp = 0.0;
							bool bb = PunchRadiusRect(*pborder, iToolType, bUseNibbling, dTmp);
#ifdef COMPILING_CAM_COMBI_DLL
							resnor= bktResNor;
#endif
							return (bb ? 1 : 0);
						} 

						// altrimenti potrebbe essere un demi rettangolo smussato: poiche' 
						// e` sempre una parte di un bordo, non e` possibile che in questa fase
						// tutto il bordo venga risolto con un unico colpo. 
						// Quindi, se il tipo di utensile e` incompatibile
						// con l'utilizzo in roditura, esco subito
						if (!bUseNibbling || iToolType != CgTool::TYPE_PUNCH)
						{
#ifdef COMPILING_CAM_COMBI_DLL
							resnor= bktResNor;
#endif
							return 0;
						}

						pborder->SetCurrItemPos(posNext);
						CgItem *pNextNextItem = pborder->GetNextItem();
						if (pNextNextItem->GetType() == CgItem::TYPE_ARC) {
							CgArc *pNextArc = (CgArc *)pNextNextItem;
							if (pNextArc->GetSignedAngle() == CbSignedAngle(M_PI_2) &&
								IsSameValue(pArc->GetRadius(), pNextArc->GetRadius())) {
								// Chiamata alla routine che cerca di punzonare la semiasola
								if (PunchSemiRadiusRect(*pborder, pos, iToolType, bUseNibbling, pSavedBorder))
								{
									// Modifica del bordo originale inserita nella routine che genera le 
									// punzonature.
									bBorderWasModified = true;
									bArcIsSolved = true;
									pArc = NULL;
								}
							}
						}
					}
				}
				
#ifdef COMPILING_CAM_COMBI_DLL
				resnor= bktResNor;
#endif
				// Riprendo la posizione dell'arco o dell'ultimo segmento della spezzata
				// di sostituzione per andare avanti
				pborder->SetCurrItemPos(pos);
			}
			pCurItem = pborder->GetNextItem();

		}
	}
	// Se sono arrivato in fondo, sicuramente il bordo non e` risolto o e` risolto
	// parzialmente, quindi ritorno false
	return (bAbort ? -1 : 0);
}

//----------------------------------------------------------------------
//
// FUNCTION:		PunchRadiusRect
//
// ARGUMENTS:	border - bordo rettangolare smussato
//				iToolType - tipo di utensile
//				bUseNibbling - uso utensile in roditura
//				dLineMax - lunghezza del segmento che definisce il rettangolo smussato piu' lungo
//							(mentre border corrisponde a quello piu' corto).
//						   In uscita vale la lunghezza "lineare" rosa dalla istruzione programmata.
//				bForSemiRadiused - true se si sta lavorando per semi rettangoli
//				dLenMax - altezza massima del rettangolo in caso di semi rettangoli
//
// RETURN VALUE: true se la punzonatura e` completata, false altrimenti
//
// AUTHOR(S):   N. Tognon
//
// DATE:		10/2010
//
// DESCRIPTION:  Punzonatura di un bordo rettangolare smussato
//
//----------------------------------------------------------------------
bool 
CArcsBorderWorkStrategy::PunchRadiusRect(CAMBORDER &border, int iToolType, bool bUseNibbling, double &dLineMax, 
										 bool bForSemiRadiused /*= false*/, double dLenMax /*= 0.0*/)
{
#ifdef COMPILING_CAMPUNCH_DLL
	CCAD *pCurrCAD = pCamPunchCAD;
#endif
#ifdef COMPILING_CAM_COMBI_DLL
	CCAD *pCurrCAD = pCmbCadHndlr;
#endif

	double			dRadius = 0.0,
					dWidth = 0.0,
					dHeight = DBL_MAX;
	CbSignedAngle	slope;
	CbPoint			ptStart;

	// Ricavo i dati del bordo rettangolare smussato

	if (!border.IsRadiussedRectangle())
		return false;

	CgItem *pItem = border.GetFirstItem();
	if (pItem->GetType() != CgItem::TYPE_ARC) {
		pItem = border.GetNextItem();
	}
	if (bForSemiRadiused) {
		// Considero altezza minima
		pItem = border.GetPrevItem();
		dHeight = pItem->GetLen();
		pItem = border.GetNextItem();
	}
	CgArc *pArc = (CgArc *)pItem;
	dRadius = pArc->GetRadius();
	pItem = border.GetNextItem();
	CgLine *pL = (CgLine *)pItem;
	slope = CbSignedAngle(pL->GetAngle().Get());
	dWidth = pL->GetLen();
	pItem = border.GetNextItem();
	// salto il prossimo arco
	pItem = border.GetNextItem();
	dHeight = __min(dHeight, pItem->GetLen());
	ptStart = pL->GetStart();
	ptStart -= CbVector(dRadius, slope);

	dWidth += 2*dRadius;
	dHeight += 2*dRadius;

	CbPoint ptEnd = ptStart + CbVector(dWidth, slope) + CbVector(dHeight, slope+M_PI_2);

	CbPoint ptEndMax(ptEnd);
	if (IsGreater(dLenMax, dHeight)) {
		ptEndMax = ptStart + CbVector(dWidth, slope) + CbVector(dLenMax, slope+M_PI_2);
	}

	CbMatrix mRot;
	mRot.SetZRotation(CbSignedAngle(-slope.Get()));
	CbPoint ptCenter = border.GetCenter();

	ptStart = ptCenter + ((ptStart-ptCenter) * mRot);
	ptEnd = ptCenter + ((ptEnd-ptCenter) * mRot);
	ptEndMax = ptCenter + ((ptEndMax-ptCenter) * mRot);


	CbVarRect var(iToolType, &border, NULL, &GET_TOOL_DB, &GETPUNCHPROGRAM);
	var.SetRadiusing(true, dRadius);
	var.SetMin(ptStart, ptEnd);
	var.SetMax(ptStart, ptEndMax);

	bool bIsSolved = false;
	CgTool *pToolUsed = NULL;
	PUNCH_INSTRUCTION *pFoundPnc = NULL;
	CbBox outBox(GetPart()->GetBorder(0)->GetBBox());

	bool bWorkOnNotches = !border.IsOriginal();
	bool bPreferXYP = (border.GetType() == TYPE_HOLE_FOR_GROUP);

	int iAns = var.CalcS4Statement(bWorkOnNotches, outBox, slope, 
									ptCenter, iToolType, true, bPreferXYP,
									bIsSolved, false, false,
									pToolUsed, pFoundPnc);

	if (bIsSolved && pFoundPnc == NULL)
		bIsSolved = false;

	if (bIsSolved) {
		GETPUNCHPROGRAM.SAVE_PUNCH_OPERATION(pFoundPnc, &border);
		if (bForSemiRadiused)
			dLineMax = dWidth;
	}

	return bIsSolved;
}

//----------------------------------------------------------------------
//
// FUNCTION:		PunchSemiRadiusRect
//
// ARGUMENTS:	border - bordo da lavorare
//               posArc - posizione del primo arco del semi rettangolo smussato nel bordo
//				iToolType - tipo di utensile
//				bUseNibbling - uso utensile in roditura
//				pSavedBorder - puntatore al bordo originale, prima di eventuali modifiche.
//
// RETURN VALUE: true se la punzonatura e` completata, false altrimenti
//
// AUTHOR(S):    E. Bertuzzi
//
// DATE:			05/98
//
// DESCRIPTION:  Punzonatura di una semiasola di un bordo
//
//----------------------------------------------------------------------
bool 
CArcsBorderWorkStrategy::PunchSemiRadiusRect(CAMBORDER &border, POSITION &posArc, int iToolType, 
										  bool bUseNibbling, CgBorder *pSavedBorder)
{
#ifdef COMPILING_CAMPUNCH_DLL
	CCAD *pCurrCAD = pCamPunchCAD;
#endif
#ifdef COMPILING_CAM_COMBI_DLL
	CCAD *pCurrCAD = pCmbCadHndlr;
#endif

	if (0)
	{
		border.SetPen(3);
		border.Show();
		pISM->WaitForUser();

	}
	border.SetCurrItemPos(posArc);
	CgArc *pArc = (CgArc *) border.GetCurrItem();
	double dRad = pArc->GetRadius();
	CbPoint c = pArc->GetCenter();

	// Lunghezza del segmento piu` corto fra i due segmenti paralleli del semi rettangolo smussato
	CgLine *pLine = (CgLine *) border.GetPrevItem();
	double dLineLen = pLine->GetLen();

	border.GetNextItem();
	// Recupero linea tra i due archi a 90 gradi di stesso raggio
	pLine = (CgLine *) border.GetNextItem();

	// Inclinazione della semi rettangolo smussato
	CbAngle ang = pLine->GetDir().Angle();
	double dWidth = pLine->GetLen();
	CbPoint ptStart = pLine->GetStart();

	// Recupero linea successiva al secondo arco
	border.GetNextItem();
	pLine = (CgLine *) border.GetNextItem();

	// Memorizzo anche la lunghezza max tra le due linee, in maniera che poi verifichero' se conviene
	// eseguire l'asola piu' lunga per rodere contemporaneamente tutta la linea piu' lunga.
	double dLineMax = mymax(dLineLen, pLine->GetLen());

	dLineLen = mymin(dLineLen, pLine->GetLen());

	// Ora costruisco un bordo a rettangolo smussato, controllo che tale bordo non
	// interferisca col bordo originale, e poi lo risolvo utilizzando la routine
	// che punzona i singoli bordi smussati. Infine allineo le strutture in modo che gli statement
	// agganciati al bordo fittizio passino al bordo originale.
	// Costruisco intanto un bordo con offset, per controllare le intersezioni
	CgBorder borRadiusRect;
	CgArc	arc1 (c, dRad - CAMPUNCH_OFFSET_BORDER, pArc->GetStartVect(), CbSignedAngle(M_PI_2));
	CgLine	line1 (arc1.GetEnd(), dWidth , ang);
	CgArc	arc2 (c + CbUnitVector(ang) * (dWidth ), dRad - CAMPUNCH_OFFSET_BORDER, pArc->GetEndVect(), CbSignedAngle(M_PI_2));
	CgLine	line2 (arc2.GetEnd(), dLineLen, ang+M_PI_2);
	CgArc	arc3 (arc2.GetCenter() + CbUnitVector(ang+M_PI_2) * dLineLen, dRad - CAMPUNCH_OFFSET_BORDER, arc2.GetEndVect(), CbSignedAngle(M_PI_2));
	CgLine	line3 (arc3.GetEnd(), dWidth, ang+M_PI);
	CgArc	arc4 (arc3.GetCenter() + CbUnitVector(ang+M_PI) * (dWidth ), dRad - CAMPUNCH_OFFSET_BORDER, arc3.GetEndVect(), CbSignedAngle(M_PI_2));
	CgLine	line4 (arc4.GetEnd(), arc1.GetStart());
	borRadiusRect.SetAutoCheck(false);
	borRadiusRect.AddItem(&arc1);
	borRadiusRect.AddItem(&line1);
	borRadiusRect.AddItem(&arc2);
	borRadiusRect.AddItem(&line2);
	borRadiusRect.AddItem(&arc3);
	borRadiusRect.AddItem(&line3);
	borRadiusRect.AddItem(&arc4);
	borRadiusRect.AddItem(&line4);

	CbPointArray apt;
	if(0)
	{
		borRadiusRect.SetPen(4);
		borRadiusRect.Show();
		pISM->WaitForUser();

	}
	int nNumInt = Intersect(borRadiusRect, *pSavedBorder, apt);
	borRadiusRect.Reset();
	if (nNumInt != 0) {
		dLineLen -= dRad + CAMPUNCH_OFFSET_BORDER;
		borRadiusRect.SetAutoCheck(false);
		borRadiusRect.AddItem(&arc1);
		borRadiusRect.AddItem(&line1);
		borRadiusRect.AddItem(&arc2);
		CgLine line2bis(arc2.GetEnd(), dLineLen, ang+M_PI_2);
		CgArc arc3bis(arc2.GetCenter() + CbUnitVector(ang+M_PI_2) * dLineLen, dRad - CAMPUNCH_OFFSET_BORDER, arc2.GetEndVect(), CbSignedAngle(M_PI_2));
		CgLine line3bis(arc3bis.GetEnd(), dWidth , ang+M_PI);
		CgArc arc4bis(arc3bis.GetCenter() + CbUnitVector(ang+M_PI) * (dWidth  ), dRad - CAMPUNCH_OFFSET_BORDER, arc3bis.GetEndVect(), CbSignedAngle(M_PI_2));
		CgLine line4bis(arc4bis.GetEnd(), arc1.GetStart());
		borRadiusRect.AddItem(&line2bis);
		borRadiusRect.AddItem(&arc3bis);
		borRadiusRect.AddItem(&line3bis);
		borRadiusRect.AddItem(&arc4bis);
		borRadiusRect.AddItem(&line4bis);
		nNumInt = Intersect(borRadiusRect, *pSavedBorder, apt);

		borRadiusRect.Reset();
		if (nNumInt != 0) {
			return false;
		}
	}

	// Verifico se e' possibile arrivare fino alla max lunghezza tra linee parallele che definiscono 
	// il semi rettangolo smussato.
	double dLenPunch = dLineMax;
	if (IsGreater(dLineMax, dLineLen)) {
		borRadiusRect.Reset();
		line2.SetEnd(line2.GetStart() + CbVector(dLineMax,ang));
		arc3.SetCenter(c + CbVector(dLineMax, ang+M_PI_2));
		line3.SetStart(arc3.GetEnd());
		line3.SetEnd(arc3.GetEnd() + CbVector(dLineMax, ang+M_PI));
		arc4.SetCenter(arc3.GetCenter() + CbVector(dWidth, ang+M_PI));
		line4.SetStart(arc4.GetEnd());
		borRadiusRect.SetAutoCheck(false);
		borRadiusRect.AddItem(&arc1);
		borRadiusRect.AddItem(&line1);
		borRadiusRect.AddItem(&arc2);
		borRadiusRect.AddItem(&line2);
		borRadiusRect.AddItem(&arc3);
		borRadiusRect.AddItem(&line3);
		borRadiusRect.AddItem(&arc4);
		borRadiusRect.AddItem(&line4);
		if (Intersect(borRadiusRect, *pSavedBorder, apt) != 0)
			dLenPunch = 0.0;
	}
	else
		dLenPunch = 0.0;

	borRadiusRect.Reset();

	// Visto che il bordo con offset non interferisce, costruisco il bordo ad
	// asola con dimensioni esatte e poi lo passo alla routine che punzona l'asola
	CAMBORDER borRadiusRectExact;
	CgArc arcExact1 (c, dRad, pArc->GetStartVect(), CbSignedAngle(M_PI_2));
	CgLine	lineExact1 (arcExact1.GetEnd(), dWidth, ang);
	CgArc	arcExact2 (c + CbUnitVector(ang) * dWidth, dRad, pArc->GetEndVect(), CbSignedAngle(M_PI_2));
	CgLine	lineExact2 (arcExact2.GetEnd(), dLineLen, ang+M_PI_2);
	CgArc	arcExact3 (arcExact2.GetCenter() + CbUnitVector(ang+M_PI_2) * dLineLen, dRad, arcExact2.GetEndVect(), CbSignedAngle(M_PI_2));
	CgLine	lineExact3 (arcExact3.GetEnd(), dWidth, ang+M_PI);
	CgArc	arcExact4 (arcExact3.GetCenter() + CbUnitVector(ang+M_PI) * dWidth, dRad, arcExact3.GetEndVect(), CbSignedAngle(M_PI_2));
	CgLine	lineExact4 (arcExact4.GetEnd(), arcExact1.GetStart());
	borRadiusRectExact.SetAutoCheck(false);
	borRadiusRectExact.AddItem(&arcExact1);
	borRadiusRectExact.AddItem(&lineExact1);
	borRadiusRectExact.AddItem(&arcExact2);
	borRadiusRectExact.AddItem(&lineExact2);
	borRadiusRectExact.AddItem(&arcExact3);
	borRadiusRectExact.AddItem(&lineExact3);
	borRadiusRectExact.AddItem(&arcExact4);
	borRadiusRectExact.AddItem(&lineExact4);

	// Poiche' sto esaminando un semi rettangolo smussato, si deve calcolare la lunghezza max del rettangolosmussatio compatibile 
	// con il bordo (noto che quella minima e' data da borRadiusRectExact). Questo perche' si deve utilizzare 
	// un rettangolo smussato anche se esso punzona piu' del minimo necessario.
	double dLenMax = 100.0/pCurrCAD->GetUnitScale();
	CgLine line2Max(arc2.GetEnd(), dLenMax, ang+M_PI_2);
	CgArc	arc3Max (arc2.GetCenter() + CbUnitVector(ang+M_PI_2) * dLenMax, dRad - CAMPUNCH_OFFSET_BORDER, arc2.GetEndVect(), CbSignedAngle(M_PI_2));
	CgLine	line3Max (arc3Max.GetEnd(), dWidth, ang+M_PI);
	CgArc	arc4Max (arc3Max.GetCenter() + CbUnitVector(ang+M_PI) * dWidth, dRad - CAMPUNCH_OFFSET_BORDER, arc3Max.GetEndVect(), CbSignedAngle(M_PI_2));
	CgLine	line4Max (arc4Max.GetEnd(), arc1.GetStart());

	CgBorder borRadiusRectMax;
	borRadiusRectMax.SetAutoCheck(false);
	borRadiusRectMax.AddItem(&arc1);
	borRadiusRectMax.AddItem(&line1);
	borRadiusRectMax.AddItem(&arc2);
	borRadiusRectMax.AddItem(&line2Max);
	borRadiusRectMax.AddItem(&arc3Max);
	borRadiusRectMax.AddItem(&line3Max);
	borRadiusRectMax.AddItem(&arc4Max);
	borRadiusRectMax.AddItem(&line4Max);
	if (0)
	{
		borRadiusRectMax.SetPen(3);
		borRadiusRectMax.Show();
		pSavedBorder->SetPen(9);
		pSavedBorder->Show();
		pISM->WaitForUser();
	}
	int nInts = Intersect(borRadiusRectMax, *pSavedBorder, apt);
	while (nInts != 0 && IsGreater(dLenMax,dLineLen)) {
		for (int k=0; k<nInts; k++) {
			dLenMax = mymin(dLenMax,Distance(apt[k],CgLine(arc1.GetStart(), arc2.GetEnd())));
		}
		dLenMax -= CAMPUNCH_OFFSET_BORDER;
		borRadiusRectMax.Reset();
		line2Max.SetEnd(arc2.GetEnd() + CbVector(dLenMax, ang+M_PI_2));
		arc3Max.SetCenter(arc2.GetCenter() + CbUnitVector(ang+M_PI_2) * dLenMax);
		line3Max.SetStart(arc3Max.GetEnd());
		line3Max.SetEnd(arc3Max.GetEnd() + CbVector(dWidth, ang+M_PI));
		arc4Max.SetCenter(arc3Max.GetCenter() + CbUnitVector(ang+M_PI) * dWidth);
		line4Max.SetStart(arc4Max.GetEnd());
		borRadiusRectMax.SetAutoCheck(false);
		borRadiusRectMax.AddItem(&arc1);
		borRadiusRectMax.AddItem(&line1);
		borRadiusRectMax.AddItem(&arc2);
		borRadiusRectMax.AddItem(&line2Max);
		borRadiusRectMax.AddItem(&arc3Max);
		borRadiusRectMax.AddItem(&line3Max);
		borRadiusRectMax.AddItem(&arc4Max);
		borRadiusRectMax.AddItem(&line4Max);
		nInts = Intersect(borRadiusRectMax, *pSavedBorder, apt);
	}

	borRadiusRectMax.Reset();

	if (!PunchRadiusRect(borRadiusRectExact, iToolType, bUseNibbling, dLenPunch, true, dLenMax)) {
		borRadiusRectExact.Reset();
		return false;
	}

	// Allineamento statements-bordi
	PUNCH_INSTRUCTION_LIST* pWIListLocal = borRadiusRectExact.GET_PUNCH_OPERATIONS;
	for (POSITION pos = pWIListLocal->GetHeadPosition(); pos != NULL; )
	{
		PUNCH_INSTRUCTION *pWILocal = pWIListLocal->GetNext(pos);
		PUNCH_INSTRUCTION_LIST* pWIList = ((CAMBORDER*)&border)->GET_PUNCH_OPERATIONS;
		pWIList->AddTail(pWILocal);
		// Aggiungo il bordo corrente alla lista dei bordi associati
		// alla macro che punzona l'asola e tolgo da questa lista
		// il bordo fittizio
		CgBaseBorderArray* pBorArrayLocal = pWILocal->GetWorkedBorders();
		pBorArrayLocal->RemoveAt(0);
		pBorArrayLocal->Add(&border);
	}

	// Devo modificare il bordo per tenere conto delle punzonature inserite.
	if (IsGreater(dLenPunch, dLineLen)) {
		// Modifico il bordo borObrExact per averlo pari alla punzonatura.
		borRadiusRectExact.Reset();
		lineExact2.SetEnd(arcExact2.GetEnd() + CbVector(dLenPunch, ang+M_PI_2));
		arcExact3.SetCenter(arcExact2.GetCenter() + CbVector(dLenPunch, ang+M_PI_2));
		lineExact3.SetStart(arcExact3.GetEnd());
		lineExact3.SetEnd(arcExact3.GetEnd() + CbVector(dWidth, ang+M_PI));
		arcExact4.SetCenter(arcExact3.GetCenter() + CbVector(dWidth, ang+M_PI));
		lineExact4.SetStart(arcExact4.GetEnd());
		borRadiusRectExact.AddItem(&arcExact1);
		borRadiusRectExact.AddItem(&lineExact1);
		borRadiusRectExact.AddItem(&arcExact2);
		borRadiusRectExact.AddItem(&lineExact2);
		borRadiusRectExact.AddItem(&arcExact3);
		borRadiusRectExact.AddItem(&lineExact3);
		borRadiusRectExact.AddItem(&arcExact4);
		borRadiusRectExact.AddItem(&lineExact4);
	}

	// Prendo l'item precedente e seguente e le loro posizioni
	border.SetCurrItemPos(posArc);
	CgItem *pPrevItem = border.GetPrevItem();
	POSITION posPrev = border.GetCurrItemPos();
	CgItem *pFatherPrevItem;
	POSITION posFatherPrev;
	if (IsLessEqual(((CgLine *)pPrevItem)->GetLen(),dLenPunch)) {
		// Ha senso considerare il precedente del precedente.
		pFatherPrevItem = border.GetPrevItem();
		posFatherPrev = border.GetCurrItemPos();

	} else {
		pFatherPrevItem = pPrevItem;
		posFatherPrev = posPrev;
	}
	border.SetCurrItemPos(posArc);
	border.GetNextItem();				// linea tra i due archi risolti
	border.GetNextItem();				// secondo arco risolto
	CgItem *pNextItem = border.GetNextItem();
	POSITION posNext = border.GetCurrItemPos();
	CgItem *pSonNextItem;
	POSITION posSonNext;
	if (IsLessEqual(((CgLine *)pNextItem)->GetLen(),dLenPunch)) {
		// Ha senso considerare il seguente del seguente
		pSonNextItem = border.GetNextItem();
		posSonNext = border.GetCurrItemPos();
	} else {
		pSonNextItem = pNextItem;
		posSonNext = posNext;
	}

	// La sostituzione controlla innanzitutto i due item "esterni" al semi rettangolo smussato, cioe' pFatherPrevItem
	// e pSonNextItem. Qualora essi siano allineati (cioe' se segmenti, appartenenti alla stessa retta,
	// se archi, appartenenti allo stesso cerchio), allora si verifica se la loro prosecuzione che li 
	// "unisce" e' all'interno del rettangolo tranciato. In tal caso si inserisce tale prosecuzione.
	// Se invece non sono allineati, si verifica la possibilita' di allungarli fino a farli intersecare.
	// Se i due item cosi' determinati sono interni al rettangolo tranciato, essi vengono inseriti 
	// al posto del semi rettangolo smussato.
	// Qualora tale procedimento non abbia avuto successo, si inseriscono nel bordo uno o due segmenti 
	// paralleli agli assi X e Y che uniscano in maniera consistente pPrevItem e pNextItem.

	int nFound = 0;
	CgItem *pFirstNew = NULL;
	CgItem *pSecondNew = NULL;
	if (pFatherPrevItem->GetType() == CgItem::TYPE_ARC) {
		if (pSonNextItem->GetType() == CgItem::TYPE_ARC) {
			// In questo caso  sono sicuramente "esterni" alla semiasola.
			// Potrebbero essere due parti di uno stesso cerchio (oppure anche lo stesso item, nel caso
			// del foro della serratura.
			CgArc *pFatherArc = (CgArc *)pFatherPrevItem;
			CgArc *pSonArc = (CgArc *)pSonNextItem;
			if (IsLessEqual(Distance(pFatherArc->GetCenter(), pSonArc->GetCenter()),0.0) &&
				IsLessEqual(fabs(pFatherArc->GetRadius()-pSonArc->GetRadius()),resabs)) {
				// Archi appartenenti allo stesso cerchio.
				bool IsClockWise = true;
				if (pFatherArc->GetSignedAngle() < 0)
					IsClockWise = false;
				CgArc SubsArc(pFatherArc->GetCenter(), pFatherArc->GetEnd(), pSonArc->GetStart(), IsClockWise);
				CgBorder bTmp;
				bTmp.AddItem(&SubsArc);

				bool bGoingOut = CUtilsGeom::GoBorderOutBorder(bTmp, borRadiusRectExact);
				bTmp.Reset();
				if (!bGoingOut) {
					// Basta unire i due archi, cioe' inserire SubsArc.
					CgItem *pIns;
					int nNumDelEle = 7;
					if (pFatherArc == pSonArc) {
						CgCircle *pC = new CgCircle(pFatherArc->GetCenter(), pFatherArc->GetRadius());
						border.DeleteAllItems();
						//border.Reset();
						border.AddItem(pC);

						border.GotoFirstItem();
					}
					else {
						CgArc *pNewArc = new CgArc(pFatherArc->GetCenter(), pFatherArc->GetStart(), 
													pSonArc->GetEnd(), IsClockWise);
						pIns = (CgItem *)pNewArc;
						border.SetCurrItemPos(posPrev);
						border.GetPrevItem();
						border.ReplaceItems(nNumDelEle, pIns, true);
					}
					posArc = border.GetCurrItemPos();
					nFound = 1;
				}
			}
		}
	}
	else if (pSonNextItem->GetType() == CgItem::TYPE_LINE) {
		// Potrebbero essere due parti di una stessa retta.
		// Attenzione: possono essere sia elementi della semiasola che quelli immediatamente
		// precedente e seguente.
		CgLine *pFatherLine = (CgLine *)pFatherPrevItem;
		CgLine *pSonLine = (CgLine *)pSonNextItem;
		if (IsColinear(*pFatherLine, *pSonLine)) {
			// Segmenti appartenenti alla stessa retta: posso sostituire le semiasola con il 
			// segmento che li unisce?
			// In questo caso  sono sicuramente "esterni" alla semiasola.
			CgLine SubsLine(pFatherLine->GetEnd(), pSonLine->GetStart());
			CgBorder bTmp;
			bTmp.AddItem(&SubsLine);
			bool bGoingOut = CUtilsGeom::GoBorderOutBorder(bTmp, borRadiusRectExact);
			bTmp.Reset();
			if (!bGoingOut) {
				// Basta unire i due segmenti, cioe' inserire SubsLine.
				CgLine *pNewLine = new CgLine(pFatherLine->GetEnd(), pSonLine->GetStart());
				border.SetCurrItemPos(posPrev);
				border.ReplaceItems(5, pNewLine, true);
				posArc = border.GetCurrItemPos();
				nFound = 1;
			}
		}
	}
	if (nFound == 1) {
		// Sostituzione gia' eseguita: ho finito
		// Elimino istanze degli item dal bordo d'appoggio utilizzato.
		borRadiusRectExact.Reset();
		return true;
	}

	// Se gli item precedente e seguente ai due archi di 90 gradi sono paralleli ad un asse 
	// (e visto che sono paralleli tra loro basta verificarne uno) allora mi conviene
	// sostituire solo la tripletta arco-linea-arco con un segmento a loro perpendicolare in maniera da 
	// lasciare piu' liberi possibile i metodi di risoluzione seguenti (e fidando
	// che eventuali punzonature inutili vengano eliminate).
	CgLine *pPrevLine = (CgLine *)pPrevItem;
	if (IsBiparallel(pPrevLine->GetDir(), X_AXIS) || IsBiparallel(pPrevLine->GetDir(), Y_AXIS)) {
		CgBorder MyBor;
		CgLine *pL = new CgLine(pPrevLine->GetEnd(), pNextItem->GetStart());
		MyBor.AddItem(pL);
		border.SetCurrItemPos(posArc);
		border.ReplaceItems(3, &MyBor, true);
		MyBor.DeleteAllItems();
		posArc = border.GetCurrItemPos();
		// Sostituzione gia' eseguita: ho finito
		// Elimino istanze degli item dal bordo d'appoggio utilizzato.
		borRadiusRectExact.Reset();
		return true;
	}

	// Attenzione: possono essere sia elementi della semiasola che quelli immediatamente
	// precedente e seguente.
	// Verifico se posso prolungare pFatherPrevItem e pSonNextItem fino a farli intersecare.
	if (pFatherPrevItem->GetType() == CgItem::TYPE_ARC) {
		CgArc *pFather = (CgArc *)pFatherPrevItem;
		if (pSonNextItem->GetType() == CgItem::TYPE_ARC) {
			// In questo caso  sono sicuramente "esterni" alla semiasola.
			CgArc *pSon = (CgArc *)pSonNextItem;
			CbPointArray aInts;
			int nNumInts = Intersect(*pFather,*pSon,aInts,false);
			if (nNumInts > 0) {
				CbPoint Pint(aInts[0]);
				for (int j=1; j<nNumInts; j++) {
					if (IsLess(Distance(aInts[j],pFather->GetEnd()),Distance(Pint,pFather->GetEnd())))
						Pint = aInts[j];
				}
				bool IsClockWise1 = true;
				if (pFather->GetSignedAngle() < 0)
					IsClockWise1 = false;
				CgArc *pFirst = new CgArc(pFather->GetCenter(), pFather->GetEnd(), Pint, IsClockWise1);
				bool IsClockWise2 = true;
				if (pSon->GetSignedAngle() < 0)
					IsClockWise2 = false;
				CgArc *pSecond = new CgArc(pSon->GetCenter(), Pint, pSon->GetStart(), IsClockWise2);
				CgBorder bTmp;
				bTmp.AddItem(pFirst);
				bTmp.AddItem(pSecond);
				bool bGoingOut = CUtilsGeom::GoBorderOutBorder(bTmp, borRadiusRectExact);
				bTmp.Reset();
				if (!bGoingOut) {
					if (pFather != pSon) {
						pSecondNew = new CgArc(pSon->GetCenter(), Pint, pSon->GetEnd(), IsClockWise2);
						pFirstNew = new CgArc(pFather->GetCenter(), pFather->GetStart(), Pint, IsClockWise1);
					}
					else {
						pFirstNew = new CgCircle(pFather->GetCenter(), pFather->GetRadius());
					}
					// Mi posiziono sul primo degli item da cancellare.
					border.SetCurrItemPos(posPrev);
					border.GetPrevItem();
					posPrev = border.GetCurrItemPos();
					nFound = -7;	// Sostituisco 7 old items con 2 new ones
				}
				pFirst->DecRef();
				pSecond->DecRef();
			}
		} else {
			CgLine *pSon = (CgLine *)pSonNextItem;
			CbPointArray aInts;
			int nNumInts = Intersect(*pFather,*pSon,aInts,false);
			if (nNumInts > 0) {
				CbPoint Pint(aInts[0]);
				for (int j=1; j<nNumInts; j++) {
					if (IsLess(Distance(aInts[j],pFather->GetEnd()),Distance(Pint,pFather->GetEnd())))
						Pint = aInts[j];
				}
				bool IsClockWise = true;
				if (pFather->GetSignedAngle() < 0)
					IsClockWise = false;
				CgArc *pFirst = new CgArc(pFather->GetCenter(), pFather->GetEnd(), Pint, IsClockWise);
				CgLine *pSecond = new CgLine(Pint, pSon->GetStart());
				CgBorder bTmp;
				bTmp.AddItem(pFirst);
				bTmp.AddItem(pSecond);
				bool bGoingOut = CUtilsGeom::GoBorderOutBorder(bTmp, borRadiusRectExact);
				bTmp.Reset();
				if (!bGoingOut) {
					pFirstNew = new CgArc(pFather->GetCenter(), pFather->GetStart(), Pint, IsClockWise);
					pSecondNew = new CgLine(Pint, pSon->GetStart());
					// Mi posiziono sul primo degli item da cancellare.
					border.SetCurrItemPos(posPrev);
					border.GetPrevItem();
					posPrev = border.GetCurrItemPos();
					nFound = -6;	// Sostituisco 4 old items con 2 new ones
				}
				pFirst->DecRef();
				pSecond->DecRef();
			}
		}
	}
	else {
		CgLine *pFather = (CgLine *)pFatherPrevItem;
		if (pSonNextItem->GetType() == CgItem::TYPE_ARC) {
			CgArc *pSon = (CgArc *)pSonNextItem;
			CbPointArray aInts;
			int nNumInts = Intersect(*pFather,*pSon,aInts,false);
			if (nNumInts > 0) {
				CbPoint Pint(aInts[0]);
				for (int j=1; j<nNumInts; j++) {
					if (IsLess(Distance(aInts[j],pFather->GetEnd()),Distance(Pint,pFather->GetEnd())))
						Pint = aInts[j];
				}
				bool IsClockWise = true;
				if (pSon->GetSignedAngle() < 0)
					IsClockWise = false;
				CgArc *pSecond = new CgArc(pSon->GetCenter(), Pint, pSon->GetStart(), IsClockWise);
				CgLine *pFirst;
				if (posPrev != posFatherPrev) {
					pFirst = new CgLine(pFather->GetEnd(), Pint);
					CgBorder bTmp;
					bTmp.AddItem(pFirst);
					bTmp.AddItem(pSecond);
					bool bGoingOut = CUtilsGeom::GoBorderOutBorder(bTmp, borRadiusRectExact);
					bTmp.Reset();
					if (!bGoingOut) {
						pSecondNew = new CgArc(pSon->GetCenter(), Pint, pSon->GetEnd(), IsClockWise);
						pFirstNew = new CgLine(pFather->GetEnd(), Pint);
						// Il primo degli item da cancellare e' posPrev.
						nFound = -6;	// Sostituisco 4 old items con 2 new ones
					}
				}
				else {
					pFirst = new CgLine(pFather->GetStart(), Pint);
					CgBorder bTmp;
					bTmp.AddItem(pFirst);
					bTmp.AddItem(pSecond);
					bool bGoingOut = CUtilsGeom::GoBorderOutBorder(bTmp, borRadiusRectExact);
					bTmp.Reset();
					if (!bGoingOut) {
						pSecondNew = new CgArc(pSon->GetCenter(), Pint, pSon->GetEnd(), IsClockWise);
						pFirstNew = new CgLine(pFather->GetStart(), Pint);
						// Il primo degli item da cancellare e' posPrev.
						nFound = -6;	// Sostituisco 4 old items con 2 new ones
					}
				}
				pFirst->DecRef();
				pSecond->DecRef();
			}
		}
		else {
			CgLine *pSon = (CgLine *)pSonNextItem;
			CbPoint Pint;
			int nNumInts = Intersect(*pFather,*pSon,Pint,false);
			if (nNumInts > 0) {
				CgLine *pFirst;
				if (posPrev != posFatherPrev)
					pFirst = new CgLine(pFather->GetEnd(), Pint);
				else
					pFirst = NULL;
				CgLine *pSecond;
				if (posNext != posSonNext)
					pSecond = new CgLine(Pint, pSon->GetStart());
				else
					pSecond = NULL;
				CgBorder bTmp;
				if (pFirst)
					bTmp.AddItem(pFirst);
				if (pSecond)
					bTmp.AddItem(pSecond);
				bool bGoingOut = true;
				if (bTmp.CountItems() != 0)
					bGoingOut = CUtilsGeom::GoBorderOutBorder(bTmp, borRadiusRectExact);
				bTmp.Reset();
				if (!bGoingOut) {
					if (pFather != pSon) {
						if (posPrev != posFatherPrev)
							pFirstNew = new CgLine(pFather->GetEnd(), Pint);
						else
							pFirstNew = new CgLine(pFather->GetStart(), Pint);
						if (posNext != posSonNext)
							pSecondNew = new CgLine(Pint, pSon->GetStart());
						else
							pSecondNew = new CgLine(Pint, pSon->GetEnd());
						}
					else {
						pFirstNew = new CgLine(pFather->GetEnd(), pSon->GetStart());
					}
					// Il primo degli item da cancellare e' posPrev.
					nFound = -5;
				}
				if (pFirst)
					pFirst->DecRef();
				if (pSecond)
					pSecond->DecRef();
			}
		}
	}

	// Elimino istanze degli item dal bordo d'appoggio utilizzato.
	borRadiusRectExact.Reset();

	if (nFound < 0) {
		// Inserisco gli items trovati al posto degli -nFound items a partire da posPrev
		CgBorder MyBor;
		MyBor.AddItem(pFirstNew);
		if (pSecondNew)
			MyBor.AddItem(pSecondNew);
		border.SetCurrItemPos(posPrev);
		border.ReplaceItems(-nFound, &MyBor, true);
		MyBor.DeleteAllItems();
		posArc = border.GetCurrItemPos();
	}
	else if (nFound == 0) {
		// Creo una spezzata che chiude il semi rettangolo ortogonalmente ai due segmenti paralleli 
		// e passa per l'estremi del segmento piu` corto fra i due
		CbPoint p1, p2;
		CgLine *pFirstLine = NULL;
		CgLine *pSecondLine = NULL;
		if (IsSameValue(((CgLine*)pPrevItem)->GetLen(),((CgLine*)pNextItem)->GetLen())) {
			p1 = ((CgLine*)pPrevItem)->GetStart();
			p2 = ((CgLine*)pNextItem)->GetEnd();
			pFirstLine = new CgLine(p1,p2);
		}
		else if (((CgLine*)pPrevItem)->GetLen() > ((CgLine*)pNextItem)->GetLen())
		{
			p2 = ((CgLine*)pNextItem)->GetEnd();
			p1 = ((CgLine*)pPrevItem)->Perp(p2);
			pSecondLine = new CgLine(p1, p2);
			pFirstLine = new CgLine(((CgLine*)pPrevItem)->GetStart(),p1);
		}
		else
		{
			p1 = ((CgLine*)pPrevItem)->GetStart();
			p2 = ((CgLine*)pNextItem)->Perp(p1);
			pFirstLine = new CgLine(p1, p2);
			pSecondLine = new CgLine(p2,((CgLine*)pNextItem)->GetEnd());
		}


		// Sostituzione della semiasola nel bordo con la spezzata.
		border.SetCurrItemPos(posPrev);
		CgBorder MyBor;
		MyBor.AddItem(pFirstLine);
		if (pSecondLine) 
			MyBor.AddItem(pSecondLine);
		border.ReplaceItems(5, &MyBor, true);
		MyBor.DeleteAllItems();
		posArc = border.GetCurrItemPos();
	}

	return true;
}

//----------------------------------------------------------------------
//
// FUNCTION:		PartialBananaMng
//
// ARGUMENTS:	border - bordo da esaminare
//				iToolType - tipo di utensile
//				bUseNibbling - uso utensile in roditura
//
// RETURN VALUE: -1 aborted, 0 altrimenti
//
// AUTHOR(S):
//
// DATE:
//
// DESCRIPTION:  Punzonatura di un archi concentrici, il maggiore da rodere all'interno,
//				 con utensili a banana montati su rotante
//
//----------------------------------------------------------------------
int 
CArcsBorderWorkStrategy::PartialBananaMng(CAMBORDER &border, int iToolType, bool bUseNibbling)
{
	double			dRad1,
					dRad2;
	CgToolSpecial	*pToolSpecial;
	CbPoint			c;
	CbAngle			angBanana;
	CbVector		vecCen;


	// Creo una copia del bordo, che non verra` modificata: invece border verra`
	// modificato sostituendo gli archi con le spezzate.
	CgBorder borderOriginal(border);

	// Lista che conterra` gli archi del bordo, che alla fine saranno eliminati
	//CgItemList listArcs;

	int iNumOfItems = border.CountItems();
	CgItemArray aArcs;
	CgItem * pCurItem = border.GetFirstItem();
	int i;
	for (i=0; i<iNumOfItems; i++)
	{
		if (pCurItem->GetType() == CgItem::TYPE_ARC)
		{
			CgArc *pArc = (CgArc *)pCurItem;
			double dStart = pArc->GetStartAngle().Get();
			double dAmp = pArc->GetAngle().Get();
			double dAmp1 = pArc->GetSignedAngle().Get();
			aArcs.Add(pCurItem);
		}
		pCurItem = border.GetNextItem();
	}


	// Loop sugli archi identificati: per ognuno verifico se fa parte di banana parziale ed in tal caso
	// cerco di risolverlo con il suo gemello
	bool bAbort = false;
	for (int i=0; i<aArcs.GetSize() && !bAbort; i++) 
	{
		if (WAIT_THREAD->IsStopped()) {
			bAbort = true;
			continue;
		}
		pCurItem = aArcs[i];

		double			dMaxDist;
		CgArc			*pArc = (CgArc*) pCurItem;

		if (pArc->GetSignedAngle() >= 0) {
			// Verifico se esiste anche il gemello
			CgArc *pTwinArc = NULL;
			CbInterval iiCommonArc;
			CbInterval iiGlobalArc;
			for (int j=0; j<aArcs.GetSize() && !bAbort && pTwinArc == NULL; j++) {
				if (i == j) 
					continue;
				CgArc *pOtherArc = (CgArc *)aArcs[j];
				if (pOtherArc->GetSignedAngle() < 0.0 &&
					IsSamePoint(pArc->GetCenter(), pOtherArc->GetCenter()) &&
					pArc->GetRadius() > pOtherArc->GetRadius()) {
						// Archi concentrici, orientati in verso opposto
						double dStart = pArc->GetStartAngle().Get();
						CbInterval iiArc(dStart, dStart+pArc->GetSignedAngle().Get());
						dStart = pOtherArc->GetStartAngle().Get();
						CbInterval iiOtherArc(dStart + pOtherArc->GetSignedAngle().Get(), dStart);
						iiCommonArc = iiArc&iiOtherArc;
						if (!iiCommonArc.IsEmpty()) {
							iiGlobalArc = iiArc|iiOtherArc;
							pTwinArc = pOtherArc;
						}
				}
			}

			if (pTwinArc != NULL) {
				// Determino parametri della banana da risolvere
				c = pArc->GetCenter();
				dRad1 = pTwinArc->GetRadius();
				dRad2 = pArc->GetRadius();

				PUNCH_INSTRUCTION *pFoundPunch = NULL;
				CbInterval iiCurrArc;
				for (int iCheck = 0; iCheck<4 && pFoundPunch == NULL; iCheck++) {
					if (iCheck == 0) {
						iiCurrArc = iiGlobalArc;
					} else if (iCheck == 1) {
						iiCurrArc = CbInterval(iiGlobalArc.Start(), iiCommonArc.End());
					} else if (iCheck == 2) {
						iiCurrArc = CbInterval(iiCommonArc.Start(), iiGlobalArc.End());
					} else {
						iiCurrArc = iiCommonArc;
					}
					vecCen = CbVector((dRad1 + dRad2) / 2.0, CbAngle(iiCurrArc.Mid()));
					angBanana = CbAngle(iiCurrArc.Length());
					// Controllo se c'e` il punzone speciale che abbia la stessa larghezza della banana e 
					// ampiezza angolare minore o uguale a quella della banana. La condizione CbCond e` sul numero
					// di bordi che compongono l'utensile.
					CgToolArray aSpecTools;
					CgImage *pImageTool;
					CgBorder *pBorTool;
					bool bAbort = false;
					int iNumOfTools = GET_TOOL_DB.FindSpecial(aSpecTools, iToolType, CbCond(1.0, 0.0, 0.0), false, false);
					int iTool;
					for (iTool=0; iTool<iNumOfTools && !bAbort && pFoundPunch == NULL; iTool++)
					{
						if (WAIT_THREAD->IsStopped()) {
							bAbort = true;
							continue;
						}
						pToolSpecial = (CgToolSpecial *)aSpecTools[iTool];

						pImageTool = pToolSpecial->GetImage();
						if (pImageTool == NULL) 
							continue;
						pBorTool = pImageTool->GetBorder(0);

						// Se l'utensile ha forma a banana, controllo se e` compatibile col bordo
						// a banana da distruggere
						if (pBorTool->IsGenericBananaShape())
						{
							double		dRadTool1, dRadTool2;
							CbAngle		angToolAntiClockwise, angToolClockwise;
							CbPoint		cenTool, p;
							CbVector	vecCenTool;

							// Prendo i dati del bordo associato al tool e li confronto col
							// bordo da distruggere. L'utensile puo` avere una ampiezza angolare piu` piccola
							// di quella del bordo a banana
							GetGenericBananaData(*pBorTool, cenTool, angToolAntiClockwise, angToolClockwise, dRadTool1, dRadTool2, vecCenTool);
				
							if (IsSameValue(dRad1, dRadTool1) && IsSameValue(dRad2, dRadTool2))
							{
								// Per ottenere la traslazione corretta dell'utensile, prendo
								// un vettore che va dall'origine al centro dei due archi che hanno il 
								// centro in comune (il centro dell'innagine del punzone e` sempre l'origine)
								CbVector vecTransl = cenTool - CbPoint(0.0, 0.0);

								// Continuo nei seguenti due casi
								// - l'ampiezza angolare del tool e` uguale a quella del bordo e
								//   l'utensile ha la stessa inclinazione o e` rotante
								// - l'ampiezza angolare del tool e` piu piccola di quella del bordo
								//   e l'utensile e` rotante ed e` di tipo compatibile con la roditura
								CbAngle angTool = angToolAntiClockwise;
								if (angToolClockwise > angToolAntiClockwise)
									angTool = angToolClockwise;
								if (IsSameAngle(angTool,angBanana,0.5*M_PI/180.0))
								{					
									// Se le inclinazioni dei due vettori sono uguali o se l'utensile e`
									// rotante, la punzonatura si puo` fare.
									// Angolo di rotazione del foro rispetto all'utensile
									CbAngle angRot = vecCen.Angle() - vecCenTool.Angle();

									if (angRot == CbAngle(0.0) || pToolSpecial->IsRotating())
									{
										// Posizione dell'utensile per fare la punzonatura (nel caso non
										// rotante)
										p = c - vecTransl;

										if (pToolSpecial->IsRotating())
										{
											// Angolo di rotazione del foro rispetto all'utensile
											angRot = vecCen.Angle() - vecCenTool.Angle();

											// Pensando all'utensile ruotato dell'angolo di rotazione, prendo
											// un vettore che va dall'origine al centro dei due archi che hanno il
											// centro in comune
											CbVector vecTranslMod (vecTransl.Len(), vecTransl.Angle() + angRot);

											// Posizione dell'utensile per fare la punzonatura
											p = c - vecTranslMod;
										}
										// Verifico se utensile e' utilizzabile in questa posizione.
										if (!CUtilsTools::IsToolUsableInPoint(GET_TOOL_DB, pToolSpecial, p))
											continue;

										CmXyp *pXyp = new CmXyp(&GETPUNCHPROGRAM);
										pXyp->SetRefPoint(p);
										pXyp->SetUsedTool(pToolSpecial);
										pXyp->SetToolAngle(angRot);

										CgImage *pXypIma = pXyp->GetImage();
										bool bGoingOut = false;
										for (int iB = 0; iB < pXypIma->CountBorders() && !bGoingOut; iB++) {
											CgBorder *pBor = pXypIma->GetBorder(iB);
											bGoingOut = CUtilsGeom::GoBorderOutBorder(*pBor, borderOriginal);
										}
										if (bGoingOut) {
											delete pXyp;
											pXyp = NULL;
										} else {
											pFoundPunch = pXyp;
										}
									}
								}
								else if ((angTool < angBanana) && pToolSpecial->IsRotating() && bUseNibbling && 
										 iToolType == CgTool::TYPE_PUNCH)
								{
									// Calcolo del centro del primo colpo della POL rispetto al centro degli
									// archi a banana aventi centro comune
									CbAngle ang1;
									if (IsSameValue(iiCurrArc.Start(), pArc->GetStartAngle().Get(), resnor))
										ang1 = iiCurrArc.Start() + angToolAntiClockwise / 2.0;
									else
										ang1 = iiCurrArc.Start() + angToolClockwise / 2.0;
									
									// Angolo di rotazione del primo colpo rispetto alla posizione base dell'utensile
									CbAngle angRot = ang1 - vecCenTool.Angle();

									// Pensando all'utensile ruotato (nell'origine) dell'angolo di rotazione, 
									// prendo un vettore che va dall'origine al centro dei due archi che hanno il
									// centro in comune
									CbVector vecTranslMod (vecTransl.Len(), vecTransl.Angle() + angRot);
									
									// Numero di punzonature necessarie
									int iNumOfPunch = (int) ceil(angBanana.Get() / angTool.Get());

									// Passo angolare 
									CbAngle angPitch;
									if (IsSameValue(iiCurrArc.End(), pArc->GetEndAngle().Get(), resnor))
										angPitch = (angBanana - angToolAntiClockwise).Get() / (iNumOfPunch - 1);
									else
										angPitch = (angBanana - angToolClockwise).Get() / (iNumOfPunch - 1);

									// costruzione della macro
									CmPol *pPol = new CmPol(&GETPUNCHPROGRAM);
									pPol->SetRefPoint(c);
									pPol->SetUsedTool(pToolSpecial);
									pPol->SetToolAngle(angRot);
									pPol->SetSlope((-vecTranslMod).Angle());
									pPol->SetAngleBetween(angPitch);
									pPol->SetWidth(vecTranslMod.Len());
									pPol->SetPunchNumber(iNumOfPunch);
									pPol->SetCEHStatus(true);
									CgImage *pPolIma = pPol->GetImage();
									bool bGoingOut = false;
									for (int iB = 0; iB < pPolIma->CountBorders() && !bGoingOut; iB++) {
										CgBorder *pBor = pPolIma->GetBorder(iB);
										bGoingOut = CUtilsGeom::GoBorderOutBorder(*pBor, borderOriginal);
									}
									if (bGoingOut) {
										delete pPol;
										pPol = NULL;
									} else {
										pFoundPunch = pPol;
									}
								}
							}
						}

						pImageTool->DeleteAllItems();
						delete pImageTool;
					}
				}

				if (pFoundPunch != NULL) {
					// Sostituisco i due archi risolti

					// cerco arco identico nel bordo originale
					CgItem *pOrig = borderOriginal.GetFirstItem();
					int j;
					for (j=0; j<borderOriginal.CountItems(); j++) {
						if (pOrig->GetType() == CgItem::TYPE_ARC) {
							CgArc *pArc1 = (CgArc*) pOrig;
							if ((IsSameValue(pArc->GetRadius(), pArc1->GetRadius()) &&
								 pArc->GetCenter() == pArc1->GetCenter() &&
								 pArc->GetStart() == pArc1->GetStart() &&
								 pArc->GetEnd() == pArc1->GetEnd()) ||
								(IsSameValue(pTwinArc->GetRadius(), pArc1->GetRadius()) &&
								 pTwinArc->GetCenter() == pArc1->GetCenter() &&
								 pTwinArc->GetStart() == pArc1->GetStart() &&
								 pTwinArc->GetEnd() == pArc1->GetEnd()))
							{
								POSITION posArcInBorder = borderOriginal.GetCurrItemPos();
								// Devo verificare se eseguito tutto l'arco: in caso contrario lascio il pezzettino/i
								// non eseguito.
								if (IsSameValue(pArc->GetRadius(), pArc1->GetRadius())) {
									// Sto considerando l'arco in senso antiorario
									if (!IsSameValue(iiCurrArc.Start(), iiGlobalArc.Start(), resnor) &&
										!IsSameAngle(pArc->GetStartAngle(), CbAngle(iiCurrArc.Start()))) {
										// Non ho completato inzio dell'arco: spezzo arco in due
										CgArc *pArcIniz = (CgArc *)pArc->Duplicate();
										pArcIniz->SetSignedAngle(CbSignedAngle(iiCurrArc.Start() - pArc->GetStartAngle().Get()));
										// Riprendo la posizione dell'arco ed inserisco il nuovo arco
										POSITION pos = border.FindItemPos(pArc);
										border.SetCurrItemPos(pos);
										CgItem *pNext = border.GetNextItem();
										border.SetCurrItemPos(pos);
										border.DeleteCurrItem(false);
										pos = border.FindItemPos(pNext);
										border.SetCurrItemPos(pos);
										pArc->SetStart(pArcIniz->GetEnd());
										border.InsertItemBefore(pArc);
										border.SetCurrItemPos(border.FindItemPos(pArc));
										border.InsertItemBefore(pArcIniz);
										// ... idem per il borderOriginal
										pArcIniz = (CgArc *)pArcIniz->Duplicate();
										borderOriginal.SetCurrItemPos(posArcInBorder);
										pNext = borderOriginal.GetNextItem();
										borderOriginal.SetCurrItemPos(posArcInBorder);
										borderOriginal.DeleteCurrItem(false);
										pos = borderOriginal.FindItemPos(pNext);
										borderOriginal.SetCurrItemPos(pos);
										pArc1->SetStart(pArcIniz->GetEnd());
										borderOriginal.InsertItemBefore(pArc1);
										borderOriginal.SetCurrItemPos(borderOriginal.FindItemPos(pArc1));
										borderOriginal.InsertItemBefore(pArcIniz);
										posArcInBorder = borderOriginal.FindItemPos(pArc1);
									}
									if (!IsSameValue(iiCurrArc.End(), iiGlobalArc.End(), resnor) &&
										!IsSameAngle(pArc->GetEndAngle(), CbAngle(iiCurrArc.End()))) {
										// Non ho completato fine dell'arco: spezzo arco in due
										CgArc *pArcFinal = (CgArc *)pArc->Duplicate();
										pArcFinal->SetStart(pArcFinal->GetCenter() + CbVector(pArcFinal->GetRadius(), CbAngle(iiCurrArc.End())));
										// Riprendo la posizione dell'arco ed inserisco il nuovo arco
										POSITION pos = border.FindItemPos(pArc);
										border.SetCurrItemPos(pos);
										CgItem *pNext = border.GetNextItem();
										border.SetCurrItemPos(pos);
										border.DeleteCurrItem(false);
										pos = border.FindItemPos(pNext);
										border.SetCurrItemPos(pos);
										border.InsertItemBefore(pArcFinal);
										border.SetCurrItemPos(border.FindItemPos(pArcFinal));
										pArc->SetEnd(pArcFinal->GetStart());
										border.InsertItemBefore(pArc);
										// ... idem per il borderOriginal
										pArcFinal = (CgArc *)pArcFinal->Duplicate();
										borderOriginal.SetCurrItemPos(posArcInBorder);
										pNext = borderOriginal.GetNextItem();
										borderOriginal.SetCurrItemPos(posArcInBorder);
										borderOriginal.DeleteCurrItem(false);
										pos = borderOriginal.FindItemPos(pNext);
										borderOriginal.SetCurrItemPos(pos);
										borderOriginal.InsertItemBefore(pArcFinal);
										borderOriginal.SetCurrItemPos(borderOriginal.FindItemPos(pArcFinal));
										pArc1->SetEnd(pArcFinal->GetStart());
										borderOriginal.InsertItemBefore(pArc1);
										posArcInBorder = borderOriginal.FindItemPos(pArc1);
									}
								} else {
									// Sto considerando l'arco in senso orario
									if (!IsSameValue(iiCurrArc.End(), iiGlobalArc.End(), resnor) &&
										!IsSameAngle(pTwinArc->GetStartAngle(), CbAngle(iiCurrArc.End()))) {
										// Non ho completato inzio dell'arco: spezzo arco in due
										CgArc *pArcIniz = (CgArc *)pTwinArc->Duplicate();
										pArcIniz->SetSignedAngle(CbSignedAngle(iiCurrArc.End()-pTwinArc->GetStartAngle().Get()));
										// Riprendo la posizione dell'arco ed inserisco il nuovo arco
										POSITION pos = border.FindItemPos(pTwinArc);
										border.SetCurrItemPos(pos);
										CgItem *pNext = border.GetNextItem();
										border.SetCurrItemPos(pos);
										border.DeleteCurrItem(false);
										pos = border.FindItemPos(pNext);
										border.SetCurrItemPos(pos);
										pTwinArc->SetStart(pArcIniz->GetEnd());
										border.InsertItemBefore(pTwinArc);
										border.SetCurrItemPos(border.FindItemPos(pTwinArc));
										border.InsertItemBefore(pArcIniz);
										// ... idem per il borderOriginal
										pArcIniz = (CgArc *)pArcIniz->Duplicate();
										borderOriginal.SetCurrItemPos(posArcInBorder);
										pNext = borderOriginal.GetNextItem();
										borderOriginal.SetCurrItemPos(posArcInBorder);
										borderOriginal.DeleteCurrItem(false);
										pos = borderOriginal.FindItemPos(pNext);
										borderOriginal.SetCurrItemPos(pos);
										pArc1->SetStart(pArcIniz->GetEnd());
										borderOriginal.InsertItemBefore(pArc1);
										borderOriginal.SetCurrItemPos(borderOriginal.FindItemPos(pArc1));
										borderOriginal.InsertItemBefore(pArcIniz);
										posArcInBorder = borderOriginal.FindItemPos(pArc1);
									}
									if (!IsSameValue(iiCurrArc.Start(), iiGlobalArc.Start(), resnor) &&
										!IsSameAngle(pTwinArc->GetEndAngle(), CbAngle(iiCurrArc.Start()))) {
										// Non ho completato fine dell'arco: spezzo arco in due
										CgArc *pArcFinal = (CgArc *)pTwinArc->Duplicate();
										pArcFinal->SetStart(pArcFinal->GetCenter() + CbVector(pArcFinal->GetRadius(), CbAngle(iiCurrArc.Start())));
										// Riprendo la posizione dell'arco ed inserisco il nuovo arco
										POSITION pos = border.FindItemPos(pTwinArc);
										border.SetCurrItemPos(pos);
										CgItem *pNext = border.GetNextItem();
										border.SetCurrItemPos(pos);
										border.DeleteCurrItem(false);
										pos = border.FindItemPos(pNext);
										border.SetCurrItemPos(pos);
										border.InsertItemBefore(pArcFinal);
										border.SetCurrItemPos(border.FindItemPos(pArcFinal));
										pTwinArc->SetEnd(pArcFinal->GetStart());
										border.InsertItemBefore(pTwinArc);
										// ... idem per il borderOriginal
										pArcFinal = (CgArc *)pArcFinal->Duplicate();
										borderOriginal.SetCurrItemPos(posArcInBorder);
										pNext = borderOriginal.GetNextItem();
										borderOriginal.SetCurrItemPos(posArcInBorder);
										borderOriginal.DeleteCurrItem(false);
										pos = borderOriginal.FindItemPos(pNext);
										borderOriginal.SetCurrItemPos(pos);
										borderOriginal.InsertItemBefore(pArcFinal);
										borderOriginal.SetCurrItemPos(borderOriginal.FindItemPos(pArcFinal));
										pArc1->SetEnd(pArcFinal->GetStart());
										borderOriginal.InsertItemBefore(pArc1);
										posArcInBorder = borderOriginal.FindItemPos(pArc1);
									}
								}
								// Creazione della spezzata di sostituzione
								CgBorder borderSubs;
								if (CUtilsGeom::CreatePolylineSubs(borderOriginal, posArcInBorder, GetParameters().dIntOverlap, borderSubs, dMaxDist) == false)
								{
									// distruzione spezzata sostituzione
									borderSubs.DeleteAllItems();
								} else {
									if (0) {
										pArc1->SetPen(9);
										pArc1->Show();
										borderSubs.SetPen(2);
										borderSubs.Show();
										pISM->WaitForUser();
										pArc1->SetPen(1);
										borderSubs.SetPen(1);
									}
									POSITION pos = NULL;
									if (IsSameValue(pArc->GetRadius(), pArc1->GetRadius())) {
										// Assegno alla spezzata lo stesso layer dell'arco
										borderSubs.SetLayer(pArc->GetLayer());
												
										// Riprendo la posizione dell'arco
										pos =	border.FindItemPos(pArc);
									} else {
										// Assegno alla spezzata lo stesso layer dell'arco
										borderSubs.SetLayer(pTwinArc->GetLayer());
												
										// Riprendo la posizione dell'arco
										pos =	border.FindItemPos(pTwinArc);
									}
									border.SetCurrItemPos(pos);

									// Sostituzione dell'arco con la spezzata
									border.GetNextItem();
									POSITION POSTMP = border.GetCurrItemPos();
									border.GetPrevItem();
									border.ReplaceItem(&borderSubs, true);
									border.SetCurrItemPos(POSTMP);

								}

								borderOriginal.SetCurrItemPos(posArcInBorder);

							}
						}
						pOrig = borderOriginal.GetNextItem();
					}
					GETPUNCHPROGRAM.SAVE_PUNCH_OPERATION(pFoundPunch, &border);
				}
			}
		}

	}

	aArcs.DeleteAllItems();

	// Eliminazione del bordo copia
	borderOriginal.DeleteAllItems();

	// Se sono arrivato in fondo, sicuramente il bordo non e` risolto o e` risolto
	// parzialmente, quindi ritorno false
	return (bAbort ? -1 : 0);
}

//----------------------------------------------------------------------
//
// FUNCTION:	GetGenericBananaData
//
// ARGUMENTS:	border - bordo a banana generica
//
// RETURN VALUE: center - centro dei due archi che hanno il centro in comune
//               angAntiClockwise - angolo dell'arco antiorario
//				 angClockwise - angolo dell'arco in senso orario
//				 dRad1 - il minore dei raggi dei due archi che hanno il centro in comune
//				 dRad2 - il maggiore dei raggi dei due archi che hanno il centro in comune
//               vecCenter - vettore che va dal centro dei due archi che hanno il centro il 
//                           comune al centro della banana
//
// AUTHOR(S):    E. Bertuzzi
//
// DATE:			06/98
//
// DESCRIPTION:  Dato un bordo a banana, restituisce i dati relativi
//
//----------------------------------------------------------------------
void 
CArcsBorderWorkStrategy::GetGenericBananaData(CgBorder &border, CbPoint &center, CbAngle &angAntiClockwise, CbAngle &angClockwise, double &dRad1, double &dRad2, CbVector &vecCenter)
{

	CbAngle		angMid;

	CgArc		*pArc1	= NULL;
	CgItem		*pItem1 = border.GetFirstItem();
	if (pItem1->GetType() == CgItem::TYPE_ARC)
		pArc1 = (CgArc *)pItem1;
	POSITION	pos1	= border.GetCurrItemPos();
	CgArc		*pArc2	= NULL;
	CgItem		*pItem2 = border.GetNextItem();
	if (pItem2->GetType() == CgItem::TYPE_ARC)
		pArc2 = (CgArc *)pItem2;
	POSITION	pos2	= border.GetCurrItemPos();
	CgArc		*pArc3	= NULL;
	CgItem		*pItem3 = border.GetNextItem();
	if (pItem3->GetType() == CgItem::TYPE_ARC)
		pArc3 = (CgArc *)pItem3;
	POSITION	pos3	= border.GetCurrItemPos();
	CgArc		*pArc4	= NULL;
	CgItem		*pItem4 = border.GetNextItem();
	if (pItem4->GetType() == CgItem::TYPE_ARC)
		pArc4 = (CgArc *)pItem4;
	POSITION	pos4	= border.GetCurrItemPos();

	// Trovo il centro dei due archi che hanno il centro in comune, i loro raggi e il
	// centro del primo arco di 180 gradi, vale a dire quell'arco da cui partirebbe
	// la roditura nel caso la banana venisse distrutta con un utensile circolare
	// avente il diametro pari alla larghezza della banana
	if (pArc1 != NULL && pArc3 != NULL && IsSamePoint(pArc1->GetCenter(), pArc3->GetCenter()))
	{
		center = pArc1->GetCenter();
		if (pArc1->GetRadius() > pArc3->GetRadius())
		{
			angAntiClockwise = pArc1->GetAngle();
			angClockwise = pArc3->GetAngle();
			angMid = pArc1->GetStartAngle() + (pArc1->GetEndAngle() - pArc1->GetStartAngle()) / 2.0;
			dRad1 = pArc3->GetRadius();
			dRad2 = pArc1->GetRadius();
		}
		else
		{
			angAntiClockwise = pArc3->GetAngle();
			angClockwise = pArc1->GetAngle();
			angMid = pArc3->GetStartAngle() + (pArc3->GetEndAngle() - pArc3->GetStartAngle()) / 2.0;
			dRad1 = pArc1->GetRadius();
			dRad2 = pArc3->GetRadius();
		}
		vecCenter = CbVector((pArc1->GetRadius() + pArc3->GetRadius()) / 2.0, angMid);
	}
	else if (pArc2 != NULL && pArc4 != NULL && IsSamePoint(pArc2->GetCenter(), pArc4->GetCenter()))
	{
		center = pArc2->GetCenter();
		if (pArc2->GetRadius() > pArc4->GetRadius())
		{
			angAntiClockwise = pArc2->GetAngle();
			angClockwise = pArc4->GetAngle();
			angMid = pArc2->GetStartAngle() + (pArc2->GetEndAngle() - pArc2->GetStartAngle()) / 2.0;
			dRad1 = pArc4->GetRadius();
			dRad2 = pArc2->GetRadius();
		}
		else
		{
			angAntiClockwise = pArc4->GetAngle();
			angClockwise = pArc2->GetAngle();
			angMid = pArc4->GetStartAngle() + (pArc4->GetEndAngle() - pArc4->GetStartAngle()) / 2.0;
			dRad1 = pArc2->GetRadius();
			dRad2 = pArc4->GetRadius();
		}
		vecCenter = CbVector((pArc2->GetRadius() + pArc4->GetRadius()) / 2.0, angMid);
	}

	return;

}


