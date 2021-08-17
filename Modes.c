// DEBUT TABLE
//******************************************************************************
// PRO: BANC DE TEST LSE
// PRG: Modes.c
// VER: Version I00A00M du 01/02/2012
// AUT: C. BERENGER / MICROTEC
//
// ROL: Programme de gestion de l'écran du mode Automatique/Manuel  ************** JUST MODE RUN : V. carolina 2019
//
// HST: Version I00A00A du 14/09/2010 / MP. LACROIX MICROTEC / Création
// HST: Version I00A00B du 15/11/2010 / C.BERENGER /
//		- Passage de la résolution moteur à 0.1 tours/min
//		- Modification de l'affichage du graphique en mode manuel
//		- Correction du problème de gestion des limites après le chargement
//	      d'un cycle dans une autre unité de vitesse ou de pression que celle
//	      utilisée par défaut.
// HST: Version I00A00C du 30/11/2010 / C. BERENGER MICROTEC /
//	    - Ajout d'une gestion par watchdog pour couper le moteur et la pression
//	      en cas de problème de blocage de l'application.
// HST: Version I00A00D du 04/01/2011 / C. BERENGER MICROTEC /
//	    - Ajout de quelques Mutex
//		- Suppresison de l'utilisation du fichier "MessageIhm.ini", et remplacement par des constantes
//		- Recompilation du programme avec la version 8.6.0f5 du DAQmx (identique à la version installée sur le banc du client)
//		- Passage de la variable "GptstConfig" en mode protégé dans le fichier "Modes.c"
// HST: Version I00A00E du 15/01/2011 / C. BERENGER MICROTEC /
//		- Modification du nombre de rééssais RS232 => Passage de 1 à 3 rééssais
// HST: Version I00A00F (20/01/2011):
//		- Modification du nombre de rééssais de communication RS232 en cas de problème
//		- Diminution du timeout RS232 de 2s à 0.3s
//
// HST: Version I00A00G (24/01/2011):
//		- Utilisation du DAQmx 9.02
//		- Suppression de l'attente entre l'écriture et la lecture sur le port RS232 (pilotage du moteur)
//
// HST: Version I00A00H (26/01/2011):
//		- Utilisation du DAQmx 9.02
//		- Simplification de l'application (suppression de mutex, AsyncTimer pour les acquisitions)
//		   => Report des mutex dans les librairies Variateur et USB6008.
//
// HST: Version I00A00I (26/06/2011):
//		- Filtrage des bruits de relectures de la pression
//
// HST: Version I00A00J (07/07/2011):
//		- Ajout d'un message d'erreur si détection de vibrations
// HST: Version I00A00M (01/02/2012):
//		- Modifications dans la fonction "ProgRampeVariateur" pour limiter les
//		  durées de rampes de vitesse à 0.4s au minimum.
//		  La prise en compte de durées inférieures provoquait des pics (overshoot)
//		  dans les rampes de vitesse.
//		- Modification dans la Callback "Configuration":
// HST: Version Carolina (25/03/2019):
//******************************************************************************
//
//******************************************************************************
// FIN TABLE

//******************************************************************************
// Inclusions
//******************************************************************************

// Librairies standards
#include <windows.h>
#include "DotNetDll.h"
#include <toolbox.h>
#include <utility.h>
#include <ansi_c.h>
#include <userint.h>
#include <cvirte.h>
#include <formatio.h>
#include <string.h>
#include <windows.h>
#include "libloaderapi.h"
#include <NIDAQmx.h>
#include <cvixml.h>
#include <pathctrl.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "CVIXML.h"
#include <math.h>

// Librairies FOURNISSEUR

// Librairies MICROTEC
#include "ZoneMessage.h"
//#include "MessageIhm.h"
#include "ResultTextFile.h"

// Librairies spécifiques à l'application
#include "IhmModes.h"
#include "GstFiles.h"
#include "Modes.h"
#include "GstTables.h"
#include "IhmAccueil.h"
#include "GstGraphe.h"
#include "Usb6008.h"
#include "Variateur.h"
#include "IhmSimul.h"
#include "Simulation.h"
#include "Mutex.h"
#include "asynctmr.h"
#include "IPC.h"
#include <cviauto.h>	 //Include
#include "excelreport.h"
#include "progressbar.h"
#include "xmlTree.h"


/********************************************************************************/
/* Macros and constants															*/
/********************************************************************************/
#ifndef xmlChk
#define xmlChk(f) if (xmlErr = (f), FAILED (xmlErr)) goto Error; else
#endif

#define MAX_SIZE 1024

//******************************************************************************
// Définition de constantes
//******************************************************************************
//----- Options -------
//#define bSIMULATION
#define bDISPLAY_ENTIRE_RESULT  0
//---------------------

#define sNOM_IHM_MODES				"IhmModes.uir"

// Couleur des courbes de consignes
#define iCOUL_CONS_VITESSE			VAL_DK_RED
#define iCOUL_CONS_PRESSION			VAL_DK_BLUE
// Couleur des courbes de relectures
#define iCOUL_VITESSE				0x00FF0000 // 0x00RRVVBB
#define iCOUL_PRESSION				0x000000FF // 0x00RRVVBB

#define iEN_COURS					1
#define iARRET						0

#define sEXT_FICHIERS				"csv"
#define sREP_CONFIGURATIONS			"Cycles"
#define sREP_RESULTATS				"Results"

#define sNOM_LIBRAIRIE 				"Modes"
#define sANUM_DLL			        "IPC.dll"
#define sANUM_CFG			        "Anum.cfg"
#define dRUN			        	4
#define dSTOP			        	5

// Format de durée d'essai écoulée
#define sFORMAT_DUREE_ECOULEE		"%dh%02dmn%02ds"

#define iFORMAT_SPEED_RESULT		"%d;%.02f;%d;%d;%.03f;%.03f;"
#define iFORMAT_PRESS_RESULT		"<-->;%d;%.02f;%d;%d;%.03f;%.03f;"

#define iGACHE_FERMEE				0
#define iGACHE_OUVERTE				1

#define dVIT_ABS_START_TEST			0.5
#define dMAX_PRESS_START_TEST		0.4  // 0.4 bars max pour autoriser le depart essai

#define dPI							3.14159
#define dGRAVITE					9.81

#define lTIMEOUT_MUTEX_PANEL		20000
#define iNB_REESSAIS_DIAL_VARIAT	6

#define COLOR_RESET  "\033[0m"
#define BOLD         "\033[1m"
#define BLACK_TEXT   "\033[30;1m"
#define RED_TEXT     "\033[31;1m"
#define GREEN_TEXT   "\033[32;1m"
#define YELLOW_TEXT  "\033[33;1m"
#define BLUE_TEXT    "\033[34;1m"
#define MAGENTA_TEXT "\033[35;1m"
#define CYAN_TEXT    "\033[36;1m"
#define WHITE_TEXT   "\033[37;1m"

#define sMSG_WARNING_QUIT	 	"Do you really want to quit ?" //Marvyn 11/06/2021 


//#define M_PI	3.14159265358979323846

//******************************************************************************
// Définition de types et structures
//******************************************************************************


//******************************************************************************
// Variables globales
//******************************************************************************


int mode = 0;
int count_pass=0;
int flag_seegraph = 0;
int Nbligne=0;
int *numeroLignePointeur=&Nbligne;  // Nb de ligne pour inserer un step
int insert_parameter=0;   // variable pour insert_parameter
int val=0;
int ExpR = 0;
int *pointeurval=&val;		 // Modif aurelien 23/01/2017 -- utilise pour test script
int *PointeurNbRowsExpR=&ExpR;   // Modif carolina 2019 -- utilise pour expected results
int RowNb = 0;
int add_tolpercent = 0;
int add_tol = 0;
int add_val = 0;
int iNumberOfRows = 0;
char cond_end2[500]="";
char ChaineExcel[200] = "=";
float Value1=0;
float Value2=0;
int EventAddScriptintoTesteSequence = 0;
char sProjectDirToTableScript2[500][500];
int val2=0;
int *pointeurvalTable2=&val2;
int ValHidePanel = 0;
Rect r;
int NBTestCheck = 0;
int contErr=0;
int Valsequencevalue = 0;
int Valonevalue = 0;
int Valrangevalue = 0;
int event_sequence = 0;

//***************MARVYN 15/06/2021****************
// counter to load initial files
int count_anum = 0;
int count_database = 0;
int count_logdir = 0;
int countModes = 0;
int insertSteps = 0 ;
int modifySteps = 0;
int numRow = 0 ;
int tableHeight = 0 ;
int command = 0;
int commandExpResult = 0;
int addChoice = 0;
int numLabel = 0;
int FirstRF = 0;
double Tend = 0 ;
int TwoLabs= 0;
int FirstLab= 0;
char *chaine;
char labelHistory[255][100]={"",""};
//har **labelHistory ;
char labelsInsert[255][100]={"",""};
int indexLab = 0;
char test[255]="";
int color = 0;
char labelModif[200]="";
int fieldNeeded =0;
struct interfaceStruct Interfaces[50];
struct column col;
int indexInterfaceStruct = 0;
char txtseqfile[MAX_PATHNAME_LEN] ="";
int execonf = 0; 
//************************************************



//MODIF MaximePAGES 22/06/2020 - E002
List * myListProcExcelPID; //list of Excel PID process.
int databasePID = 0; //database Excel PID process.

//MODIF MaximePAGES 1/07/2020 - Script Progress Bar
float currentEndTimeGlobal = 0.0;
//MODIF MaximePAGES 1/07/2020 - Script Progress Bar
int IFandIBthreshold = 0;

//Modif MaximePAGES 15/07/2020 - Seq Results Excel
int testInSeqCounter = 0;


//Modif MaximePAGES 31/07/2020 - Parameters Tab
struct Parameter  * myParameterTab = NULL;
int nbParameter = 0;

//Modif MaximePAGES 10/08/2020
int currentTxtBox = 0;

//Modif MaximePAGES 11/08/2020 
int firstCurrentSeqSaved = 0;

//Modif MaximePAGES 14/08/2020
char tabStandWUID[2][25] = {"",""};

//Modif MaximePAGES 26/08/2020
char tabDiagWUID[2][25] = {"",""};

//Modif MaximePAGES 26/08/2020
char tabSWIdentWUID[2][25] = {"",""};


//Modif MaximePAGES 18/08/2020 
struct Frame myFrameTab[100] = {""};

//MOdif MaximePAGES 27/08/2020  
int ongoingAnalyse = 0;

//Modif MAximePAGES 08/09/2020
int analyseAvailable = 0; 
int testCaseLoaded = 0; 
int logsxmlLoaded = 0; 

//**************************************************************

//******************************************************************************
// Fonction de retour du panel
//******************************************************************************
void *GFonctionRetourIhm;

//******************************************************************************
// Variables Threads
//******************************************************************************
//int  GiThreadPoolHandleAcquisitions;
int  GiThreadAcquisitions;
//int  GiThreadPoolHandleGestionVitesse;
int  GiThreadCycleVitesse;
//int  GiThreadPoolHandleGestionPression;
int  GiThreadCyclePression;
//int  GiThreadPoolHandleGestionSecurite;
int  GiThreadGestionSecurite;
int  GiThreadCycleRun;
int  GiThreadCycleAnum;

BOOL GbThreadPoolActive	= 0;
BOOL GbAnumInit = 0;

WuAutoCheck_WU_Automation_Test_and_Folder_Manager instanceLseNetModes = 0;
WuAutoCheck_LFComand *TabAnumCmd = NULL;
TAPI GapiANum;

// Tableau contenant la liste des vitesses
stTabVitesse  GstTabVitesse;
// Tableau contenant la liste des pressions
stTabPression GstTabPression;
// Pointeur vers la structure contenant lesparamètres de configuration
DefineThreadSafeScalarVar(stConfig, GptstConfig, 			10);

//******************************************************************************

char GsPathResult[500];
char GsMsgErr[3000]="";
char GsPathConfigFile[MAX_PATHNAME_LEN];
char GsPathApplication[MAX_PATHNAME_LEN];
char GsAnumCfgFile[MAX_PATHNAME_LEN]="";
char GsCheckCfgFile[MAX_PATHNAME_LEN]="";
char GsSaveConfigPathsFile[MAX_PATHNAME_LEN];
char GsLogPath[MAX_PATHNAME_LEN]="";
char GsResultsDirPath[MAX_PATHNAME_LEN]=""; 
char GsLogPathInit[MAX_PATHNAME_LEN]="";
double GdCoeffAG;



//Modif MaximePAGES 270/08/2020
char GsTestScript_Analyse[MAX_PATHNAME_LEN]="";
char GsTestScriptName_Analyse[MAX_PATHNAME_LEN]=""; 
char GsTestFolder_Analyse[MAX_PATHNAME_LEN]="";

//******************************************************************************

volatile int GiPanel			= 0;
volatile int GiPanelPere		= 0;
volatile int GiPopupAdd			= 0;
volatile int GiPopupAdd2		= 0;
volatile int GiStatusPanel		= 0;
volatile int GiGraphPanel		= 0;
volatile int GiEvtRetourIhm;
volatile int GiPopupMenuHandle			= 0;
volatile int GiPopupMenuRunHandle		= 0;
volatile int GiPopupMenuScriptHandle	= 0;		// popup script detail
volatile int GiMenuExpResultsHandle		= 0;	    //Menu Expected results panel
volatile int GiExpectedResultsPanel		= 0;	    // Panel from add_expected_results
volatile int GiPanelValue = 0;

//******************************************************************************
//database
//******************************************************************************

//MODIF MaximePAGES 22/06/2020 - Application Excel pour le projet - E002
static CAObjHandle applicationHandleProject		= 0;
//*****************************************************




//static applicationHandledata 	= 0;
static workbookHandledata 		= 0;
static worksheetHandledata	 	= 0;
static worksheetHandledata1 	= 0;
static worksheetHandledata2 	= 0;
//save file

static workbookHandlesave 		= 0;
static worksheetHandlesave1 	= 0;
static worksheetHandlesave2 	= 0;
//other
//static CAObjHandle applicationHandle 	= 0;
//static workbookHandle 		= 0;
//static worksheetHandle 		= 0;
//static CAObjHandle applicationHandle1 	= 0;
static workbookHandle1 		= 0;
static worksheetHandle1 	= 0;
//static CAObjHandle applicationHandle2 	= 0;
//static workbookHandle2 		= 0;
//static worksheetHandle2 	= 0;
//static CAObjHandle applicationHandle3 	= 0;
//static workbookHandle3 		= 0;
static worksheetHandle3 	= 0;
//unused
//static applicationHandle4 	= 0;
static workbookHandle4 		= 0;
static worksheetHandle4 	= 0;
//static CAObjHandle applicationHandle5 = 0;
//static applicationHandle5 	= 0;
static workbookHandle5 		= 0;
static worksheetHandle5 	= 0;
static workbookHandle6 		= 0;
//static CAObjHandle applicationHandle6 = 0;
//static applicationHandle6 	= 0;

//
//static CAObjHandle applicationHandle7 	= 0;
static workbookHandle7 		= 0;
static worksheetHandle7 	= 0;
//static CAObjHandle applicationHandle8 	= 0;
static workbookHandle8 		= 0;
static worksheetHandle8 	= 0;
//Logs.xlsx
//static CAObjHandle applicationHandle9 	= 0;
static workbookHandle9 		= 0;
static worksheetHandle9 	= 0;

static workbookHandle10 		= 0;
static worksheetHandle10_1 	= 0;
static worksheetHandle10_2 	= 0;

//static CAObjHandle applicationLabel		= 0;
//show sequence
static workbookHandleSeq 		= 0;
static worksheetHandleSeq 	= 0;
//static CAObjHandle applicationHandleSeq		= 0;
//load expected results
static workbookHandleLoad 		= 0;
static worksheetHandleLoad 	= 0;
//static CAObjHandle applicationHandleLoad		= 0;
//show current script
static workbookHandleCurrentScript		= 0;
static worksheetHandleCurrentScript 	= 0;
//static CAObjHandle applicationHandleCurrentScript		= 0;


static FILE *file_handle;

static int iAnumLoad = 0;	   	   // Variable AnumLoad pour test
static int iDataBaseLoad = 0;      // Variable dataBaseLoad pour test
static int iSetLogDirLoad = 0;	   // Variable SetLogDirLoad pour test


char fichierLogxml[500] = ""; //file Logs.xml
char fichierLogxlsx[500] =""; //file Logs.xlsx (excel)

//excel for sequence result analysis
static workbookHandleCoverageMatrix		= 0;
static worksheetHandleCoverageMatrix 	= 0;


//******************************************************************************
// Variables Check - Analysis of the results
//******************************************************************************
int iResultCheckSTDEV 		= 0;
int iCheckAverage			= 0;
int iResultCheckFieldValue 	= 0;
int iCheckNbofBurst 		= 0;
int iCheckNoRF 				= 0;
int iCheckTimingFirstRF 	= 0;
int iCheckNbFramesInBurst 	= 0;
int iCheckTimingInterBursts = 0;
int iCheckTimingInterFrames	= 0;
int iCheckCompareAcc	= 0;
int iCheckCompareP	= 0;

/**********************/

DefineThreadSafeScalarVar(int, GiAffMsg, 					0);
DefineThreadSafeScalarVar(int, GiUnitVitesse, 				0);
DefineThreadSafeScalarVar(int, GiUnitPression, 				0);
DefineThreadSafeScalarVar(int, GiSelectionHandlePression, 	0);
DefineThreadSafeScalarVar(int, GiSelectionHandle, 			0);
DefineThreadSafeScalarVar(int, GiControlGrapheSelection, 	0);
DefineThreadSafeScalarVar(int, GiPanelStatusDisplayed, 		0);
DefineThreadSafeScalarVar(int, GiMode, 						0);
DefineThreadSafeScalarVar(int, GbQuitter, 					0);
DefineThreadSafeScalarVar(int, GiEtat, 						0);
DefineThreadSafeScalarVar(int, GiHandleFileResult, 			0);
DefineThreadSafeScalarVar(int, GiErrUsb6008, 				0);
DefineThreadSafeScalarVar(int, GiErrVaria,   				0);
DefineThreadSafeScalarVar(int, GiIndexStepVitesse,  		0);
DefineThreadSafeScalarVar(int, GiIndexCycleVitesse, 		0);
DefineThreadSafeScalarVar(float, GfVitesse, 				0);
DefineThreadSafeScalarVar(int, GbEndCycleVitesse, 			0);
DefineThreadSafeScalarVar(int, GbRazTimeVit, 				0);
DefineThreadSafeScalarVar(int, GiDisableMajGrapheVitesse, 	0);
DefineThreadSafeScalarVar(int, GiStartCycleVitesse, 		0);
DefineThreadSafeScalarVar(int, GiSelectionHandleVitesse, 	0);
DefineThreadSafeScalarVar(double, GdVitesseAProg, 			0);
DefineThreadSafeScalarVar(double, GdDurEcoulEssaiSpeed, 	0);
DefineThreadSafeScalarVar(int, GiTimeDepartEssaiSpeed, 		0);
DefineThreadSafeScalarVar(double, GdPression, 				0);
DefineThreadSafeScalarVar(int, 	GiIndexStepPression, 		0);
DefineThreadSafeScalarVar(int, GiIndexCyclePression, 		0);
DefineThreadSafeScalarVar(int, GbRazTimePress, 				0);
DefineThreadSafeScalarVar(int, GiDisableMajGraphePression, 	0);
DefineThreadSafeScalarVar(int, GiStartVitPress, 			0);
DefineThreadSafeScalarVar(int, GiStartCyclePression, 		0);
DefineThreadSafeScalarVar(double, GdPressionAProg, 			0);
DefineThreadSafeScalarVar(double, GiTimeDepartEssaiPress, 	0);
DefineThreadSafeScalarVar(double, GdDurEcoulEssaiPress, 	0);
DefineThreadSafeScalarVar(int, GbEndCyclePression, 			0);
DefineThreadSafeScalarVar(int, GbGacheActive, 				0);
DefineThreadSafeScalarVar(int, GiStartCycleRun, 			0);
DefineThreadSafeScalarVar(int, GiStartCycleAnum, 			0);
DefineThreadSafeScalarVar(HINSTANCE, GiAnumHdll, 		 NULL);
DefineThreadSafeScalarVar(TAPI, GiAnumApi, 					0);


//******************************************************************************
// Fonctions internes au source
//******************************************************************************
void ModesAddMessageZM(char *sMsg);
static int CVICALLBACK ThreadCycleVitesse (void *functionData);
static int CVICALLBACK ThreadCyclePression (void *functionData);
static int CVICALLBACK ThreadGestionSecurite (void *functionData);
static int CVICALLBACK ThreadCycleRun (void *functionData);
static int CVICALLBACK ThreadCycleAnum (void *functionData);
//static int CVICALLBACK ThreadAcquisitions (void *functionData);
int CVICALLBACK ThreadAcquisitions (int reserved, int theTimerId, int event,
									void *callbackData, int eventData1,
									int eventData2);

static int CVICALLBACK ThreadAffMsg (void *functionData);
int SetUnitVitesse(int iPanel, int iControl);
int SetUnitPression(int iPanel, int iControl);
static int ConvertAndRenderXMLFileInTree (int panel, int tree, const char *filePath);
static int ProcessXMLElement (int panel, int tree, int itemIndex, CVIXMLElement element);
static int ProcessXMLAttribute (int panel, int tree, int itemIndex, CVIXMLAttribute attribute);

//******************************************************************************
// Corps des fonctions et procédures internes au source
//******************************************************************************
#define CHECK_ERROR(Error, Title, sMsgErr)					\
	{															\
		if (Error != 0){										\
			if(GetGiAffMsg() == 0){								\
				strcpy(sTitle, Title);							\
				strcpy(GsMsgErr, sMsgErr);						\
				SetGiStartCycleVitesse(0);						\
				SetGiStartCyclePression(0);						\
				SetGiStartCycleRun(0);							\
				SetGiStartCycleAnum(0);							\
				SetGiAnumHdll(NULL);							\
				SetGiAnumApi(0);							\
				GstIhmCycles(GiPanel,GetGiStartVitPress(),  GetGiStartCycleVitesse(), GetGiStartCyclePression());	\
				SetGiAffMsg(1);									\
				SetPanelAttribute (GiPanel, ATTR_VISIBLE, 0);	\
				if(GetGiPanelStatusDisplayed()==0){				\
					DisplayPanel (GiStatusPanel);				\
					SetGiPanelStatusDisplayed(1);				\
				}												\
				ResetTextBox (GiStatusPanel, PANEL_STAT_MESSAGES, sMsgErr);	\
			}													\
		}														\
	}


#define CHECK_TAB_INDEX(sId, iIndex, iNdexMin, iIndexMax) 	\
	{															\
		if ((iIndex < iNdexMin) || (iIndex > iIndexMax))		\
			MessagePopup(sId, "Index Error");					\
	}

#define CHECK_VAL(sIndex, dVal, dValMin, dValMax)			\
	{															\
		if ((dVal < dValMin) || (dVal > dValMax))				\
			MessagePopup(sIndex, "Val Error");					\
	}


void UpDemandeAttenteJeton (int iLine, HANDLE pthLock, long lTimeout)
{
	int iStatus;

	iStatus = DemandeAttenteJeton(pthLock, lTimeout);
}

//----------- Modif du 30/11/2010 par C.BERENGER ---- DEBUT ---------
// DEBUT ALGO
//***************************************************************************
// void ModesGetEtatCycles(int *iVitesse, int *iPression, int *iDurEssVit, int *iDurEssPress)
//***************************************************************************
//	- int *iVitesse		= Etat du cycle de vitesse
//	  int *iPression	= Etat du cycle de pression
//	  int *iDurEssVit	= Durée de l'essai de vitesse
//	  int *iDurEssPress	= Durée de l'essai de pression
//
//  - Retourne l'état des cycles de vitesse ou pression
//
//  - Aucun
//***************************************************************************
// FIN ALGO
void ModesGetEtatCycles(int *iVitesse, int *iPression, int *iDurEssVit, int *iDurEssPress)
{
	if(GbThreadPoolActive)
	{
		*iVitesse 		= GetGiStartCycleVitesse();
		*iPression 		= GetGiStartCyclePression();

		*iDurEssVit		= GetGdDurEcoulEssaiSpeed();
		*iDurEssPress	= GetGdDurEcoulEssaiPress();
	}
	else
	{
		*iVitesse 		= 0;
		*iPression 		= 0;

		*iDurEssVit		= 0.0;
		*iDurEssPress	= 0.0;
	}
}
//----------- Modif du 30/11/2010 par C.BERENGER ---- FIN ---------

// DEBUT ALGO
//***************************************************************************
// void DisplayCurrentCycle(int iPanel, char *sPathConfigFile)
//***************************************************************************
//  - int iPanel				: Handle du panel
//	  char *sPathConfigFile		: Chemin complet vers le fichier de configuration
//
//  - Affichage du cycle en cours dans le menu
//
//  - Chaîne de caractères du temps en heures/minutes/secondes
//***************************************************************************
// FIN ALGO
void DisplayCurrentCycle(int iPanel, char *sPathConfigFile)
{
	char sPathToPrint[MAX_PATHNAME_LEN*2];

	sprintf(sPathToPrint, "Current Cycle = \"%s\"", sPathConfigFile);
	//SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_CONFIG_SEE_CURR_CYCLE, ATTR_ITEM_NAME, sPathToPrint);
}

// DEBUT ALGO
//******************************************************************************
//void AffStatusPanel(int iPanel, int iStatusPanel, char *sTitle, char *sMsg,
//					int *ptiPanelStatDisplayed, int *ptiAffMsg)
//******************************************************************************
//	- int iPanel		: Handle du panel d'essai
//	  int iStatusPanel	: Handle du panel de status
//	  char *sTitle		: Titre
//	  char *sMsg		: Message à afficher
//	  int *ptiPanelStatDisplayed	: 1=Ecran déjà affiché, sinon 0
//	  int *ptiAffMsg	: 1=Message affiché, sinon, 0
//
//  - Gestion des fichiers de données (vitesse et pression)
//
//  - 0 si OK, sinon code d'erreur
//******************************************************************************
// FIN ALGO
void AffStatusPanel(int iPanel, int iStatusPanel, int iLevel, char *sTitle, char *sMsg,
					int *ptiPanelStatDisplayed, int *ptiAffMsg)
{
	int iStatus;

	if(*ptiPanelStatDisplayed == 0)
	{
		// On cache l'écran principal
		SetPanelAttribute (iPanel, ATTR_VISIBLE, 0);
		DisplayPanel(iStatusPanel);
		//Effacement de la zone de message de l'écran de status
		ResetTextBox (iStatusPanel, PANEL_STAT_MESSAGES, sTitle);

		// Modification du 07/07/2011 par CB ------ DEBUT --------------
		switch (iLevel)
		{
			case iLEVEL_WARNING:
				iStatus = SetCtrlAttribute (iStatusPanel, PANEL_STAT_MESSAGES, ATTR_TEXT_BOLD, 1);
				iStatus = SetCtrlAttribute (iStatusPanel, PANEL_STAT_MESSAGES, ATTR_TEXT_BGCOLOR, VAL_RED);
				iStatus = SetCtrlAttribute (iStatusPanel, PANEL_STAT_MESSAGES, ATTR_TEXT_COLOR,  VAL_WHITE);
				break;
			default:
			case iLEVEL_MSG:
				SetCtrlAttribute (iStatusPanel, PANEL_STAT_MESSAGES, ATTR_FRAME_COLOR, VAL_WHITE);
				SetCtrlAttribute (iStatusPanel, PANEL_STAT_MESSAGES, ATTR_TEXT_BOLD, 0);
				SetCtrlAttribute (iStatusPanel, PANEL_STAT_MESSAGES, ATTR_TEXT_BGCOLOR, VAL_WHITE);
				SetCtrlAttribute (iStatusPanel, PANEL_STAT_MESSAGES, ATTR_TEXT_COLOR,  VAL_BLACK);
				break;

		}
		// Modification du 07/07/2011 par CB ------ FIN --------------

		InsertTextBoxLine(iStatusPanel, PANEL_STAT_MESSAGES, -1, sMsg);
		*ptiPanelStatDisplayed = 1;
		*ptiAffMsg = 0;
	}
}

// DEBUT ALGO
//***************************************************************************
// void ModesAddMessageZM(char *sMsg)
//***************************************************************************
//  - sMsg: message
//
//  - Fonction ajoutant un message dans la Zone de Message
//
//  - Aucun
//***************************************************************************
// FIN ALGO
void ModesAddMessageZM(char *sMsg)
{
	int iNbLignes;
	char *ptMgs;
	char *ptMgsCopie;
	char sCopieMsg[MAX_PATHNAME_LEN*2];

	strcpy(sCopieMsg,sMsg);
	ptMgsCopie=sCopieMsg;
	// Ajout du message dans la ZM
	while ((ptMgs=strstr (ptMgsCopie,"\\n"))!=NULL)
	{
		if(ptMgs != NULL)
			*ptMgs='\0';
		ptMgs++;
		if(ptMgs != NULL)
			*ptMgs='\0';
		ptMgs++;
		InsertTextBoxLine (GiPanel, PANEL_MODE_MESSAGES_MODE,-1,ptMgsCopie);
		if(ptMgs != NULL)
			ptMgsCopie=ptMgs;
	}
	if (ptMgsCopie!=NULL)
		InsertTextBoxLine (GiPanel,PANEL_MODE_MESSAGES_MODE,-1,ptMgsCopie);
	GetNumTextBoxLines (GiPanel, PANEL_MODE_MESSAGES_MODE, &iNbLignes);

	SetCtrlAttribute (GiPanel, PANEL_MODE_MESSAGES_MODE, ATTR_FIRST_VISIBLE_LINE, iNbLignes);
	// A chaque message ajouté on vérifie s'il faut purger la ZM
	ZoneMessageSaturee(GiPanel,
					   PANEL_MODE_MESSAGES_MODE,
					   sNOM_LIBRAIRIE,
					   0);
}

// DEBUT ALGO
//******************************************************************************
//int SetLimSaisiesMan(int iPanel, int iIdVitesse, int iIdPress, int iIdTpsVitesse,
//						int iIdTpsPress, int iUnitVitesse, int iUnitPress, stConfig *ptStConfig)
//******************************************************************************
//	- int iPanel			: Handle du panel
//	  int iIdVitesse		: Numéro de contrôle de la saisie de vitesse
//	  int iIdPress		 	: Numéro de contrôle de la saisie de pression
//	  int iIdTpsVitesse		: Numéro de contrôle de la saisie du temps pour la vitesse
//	  int iIdTpsPress		: Numéro de contrôle de la saisie de temps pour la pression
//	  int iUnitVitesse		: Unité de vitesse
//	  int iUnitPress		: Unité de pression
//	  stConfig *ptStConfig	: Pointeur sur la structure des paramètres
//
//  - Fixe les limites des saisies dans le mode manuel
//
//  - 0 si OK, sinon code d'erreur
//******************************************************************************
// FIN ALGO
int SetLimSaisiesMan(int iPanel, int iIdVitesse, int iIdPress, int iIdTpsVitesse,
					 int iIdTpsPress, int iUnitVitesse, int iUnitPress, stConfig *ptStConfig)
{
	// Fixe les min/max du tableau des vitesses
	switch(iUnitVitesse)
	{
		case iUNIT_KM_H:
			SetCtrlAttribute (iPanel, iIdVitesse, ATTR_MIN_VALUE, ptStConfig->dLimMinKmH);
			SetCtrlAttribute (iPanel, iIdVitesse, ATTR_MAX_VALUE, ptStConfig->dLimMaxKmH);
			break;
		case iUNIT_G:
			SetCtrlAttribute (iPanel, iIdVitesse, ATTR_MIN_VALUE, ptStConfig->dLimMinG);
			SetCtrlAttribute (iPanel, iIdVitesse, ATTR_MAX_VALUE, ptStConfig->dLimMaxG);
			break;
		case iUNIT_TRS_MIN:
			SetCtrlAttribute (iPanel, iIdVitesse, ATTR_MIN_VALUE, ptStConfig->dLimMinTrsMin);
			SetCtrlAttribute (iPanel, iIdVitesse, ATTR_MAX_VALUE, ptStConfig->dLimMaxTrsMin);
			break;
	}
	// Limites de la durée de vitesse
	SetCtrlAttribute (iPanel, iIdTpsVitesse, ATTR_MIN_VALUE, ptStConfig->dLimMinTpsSpeed);
	SetCtrlAttribute (iPanel, iIdTpsVitesse, ATTR_MAX_VALUE, ptStConfig->dLimMaxTpsSpeed);


	// Fixe les min/max du tableau des pressions
	switch(iUnitPress)
	{
		case iUNIT_KPA:
			SetCtrlAttribute (iPanel, iIdPress, ATTR_MIN_VALUE, ptStConfig->dLimMinKpa);
			SetCtrlAttribute (iPanel, iIdPress, ATTR_MAX_VALUE, ptStConfig->dLimMaxKpa);
			break;
		case iUNIT_BAR:
			SetCtrlAttribute (iPanel, iIdPress, ATTR_MIN_VALUE, ptStConfig->dLimMinBar);
			SetCtrlAttribute (iPanel, iIdPress, ATTR_MAX_VALUE, ptStConfig->dLimMaxBar);
			break;
		case iUNIT_MBAR:
			SetCtrlAttribute (iPanel, iIdPress, ATTR_MIN_VALUE, ptStConfig->dLimMinMbar);
			SetCtrlAttribute (iPanel, iIdPress, ATTR_MAX_VALUE, ptStConfig->dLimMaxMbar);
			break;
		case iUNIT_PSI:
			SetCtrlAttribute (iPanel, iIdPress, ATTR_MIN_VALUE, ptStConfig->dLimMinPsi);
			SetCtrlAttribute (iPanel, iIdPress, ATTR_MAX_VALUE, ptStConfig->dLimMaxPsi);
			break;
	}
	// Limites de la durée de pression
	SetCtrlAttribute (iPanel, iIdTpsPress, ATTR_MIN_VALUE, ptStConfig->dLimMinTpsPress);
	SetCtrlAttribute (iPanel, iIdTpsPress, ATTR_MAX_VALUE, ptStConfig->dLimMaxTpsPress);

	return 0; // OK
}

// DEBUT ALGO
//***************************************************************************
// void ModeInitSafeVar(void)
//***************************************************************************
//  - Aucun
//
//  - Désaloue la mémoire occupée par les variables protégées
//
//  - Aucun
//***************************************************************************
// FIN ALGO
void ModeUninitSafeVar(void)
{
	UninitializeGbEndCycleVitesse ();
	UninitializeGfVitesse ();
	UninitializeGdPression ();
	UninitializeGiErrUsb6008();
	UninitializeGiErrVaria();
	UninitializeGiPanelStatusDisplayed();
	UninitializeGiIndexStepVitesse();
	UninitializeGiIndexCycleVitesse();
	UninitializeGiIndexStepPression();
	UninitializeGiIndexCyclePression();
	UninitializeGbRazTimeVit();
	UninitializeGbRazTimePress();
	UninitializeGiDisableMajGrapheVitesse();
	UninitializeGiDisableMajGraphePression();
	UninitializeGiStartCycleVitesse();
	UninitializeGiStartVitPress();
	UninitializeGiStartCyclePression();
	UninitializeGdPressionAProg();
	UninitializeGiTimeDepartEssaiPress();
	UninitializeGiSelectionHandleVitesse();
	UninitializeGdDurEcoulEssaiPress();
	UninitializeGbEndCyclePression();
	UninitializeGbGacheActive();
	UninitializeGdVitesseAProg();
	UninitializeGdDurEcoulEssaiSpeed();
	UninitializeGiTimeDepartEssaiSpeed();
	UninitializeGiHandleFileResult();
	UninitializeGiEtat();
	UninitializeGbQuitter();
	UninitializeGiSelectionHandlePression();
	UninitializeGiSelectionHandle();
	UninitializeGiControlGrapheSelection();
	UninitializeGiMode();
	UninitializeGiUnitVitesse();
	UninitializeGiUnitPression();
	UninitializeGiAffMsg();
	UninitializeGptstConfig();
	UninitializeGiStartCycleRun();
	UninitializeGiStartCycleAnum();
	UninitializeGiAnumHdll();
	UninitializeGiAnumApi();
}

// DEBUT ALGO
//***************************************************************************
// void ModeInitSafeVar(void)
//***************************************************************************
//  - Aucun
//
//  - Initialisation des pointeurs vers les variables protégées
//
//  - Aucun
//***************************************************************************
// FIN ALGO
void ModeInitSafeVar(void)
{
	InitializeGiUnitVitesse();
	InitializeGiUnitPression();
	InitializeGbEndCycleVitesse ();
	InitializeGfVitesse ();
	InitializeGdPression ();
	InitializeGiErrUsb6008();
	InitializeGiErrVaria();
	InitializeGiPanelStatusDisplayed();
	InitializeGiIndexStepVitesse();
	InitializeGiIndexCycleVitesse();
	InitializeGiIndexStepPression();
	InitializeGiIndexCyclePression();
	InitializeGbRazTimeVit();
	InitializeGbRazTimePress();
	InitializeGiDisableMajGrapheVitesse();
	InitializeGiDisableMajGraphePression();
	InitializeGiStartCycleVitesse();
	InitializeGiStartVitPress();
	InitializeGiStartCyclePression();
	InitializeGdPressionAProg();
	InitializeGiTimeDepartEssaiPress();
	InitializeGiSelectionHandleVitesse();
	InitializeGdDurEcoulEssaiPress();
	InitializeGbEndCyclePression();
	InitializeGbGacheActive();
	InitializeGdVitesseAProg();
	InitializeGdDurEcoulEssaiSpeed();
	InitializeGiTimeDepartEssaiSpeed();
	InitializeGiHandleFileResult();
	InitializeGiEtat();
	InitializeGbQuitter();
	InitializeGiSelectionHandlePression();
	InitializeGiSelectionHandle();
	InitializeGiControlGrapheSelection();
	InitializeGiMode();
	InitializeGiAffMsg();
	InitializeGptstConfig();
	InitializeGiStartCycleRun();
	InitializeGiStartCycleAnum();
	InitializeGiAnumHdll();
	InitializeGiAnumApi();

	SetGiUnitVitesse(0);
	SetGiUnitPression(0);
	SetGiHandleFileResult(-1);
}


//Modif MaximePAGES 10/08/2020 - New value and parameters insertion  ************  
int CVICALLBACK ValueTxt_Callback (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	
	int itemValue =0;
	
	if (event == EVENT_LEFT_CLICK)
	{
		if ((GiPanelValue = LoadPanelEx (0,"IhmModes.uir",PANELVALUE, __CVIUserHInst)) < 0)
		{
			return -1;
		}
		InstallPopup (GiPanelValue);

		add_val = 1;
		add_tolpercent = 0;
		add_tol = 0;
		count_pass=0;
		
		if (modifySteps != 1) 
		{
		GetCtrlVal(panel,EXPRESULTS_EXPLIST, &itemValue);
		
		}
		else
		{
		//MARVYN PANNETIER 23/07/2021
		itemValue= commandExpResult;
		}
		
		//Modif MaximePAGES 06/08/2020 ***************************
		if (itemValue == 2 || itemValue == 5 || itemValue == 6)
		{

			SetCtrlAttribute(GiPanelValue, PANELVALUE_ONEVALUE, ATTR_DIMMED,			0);
			SetCtrlAttribute(GiPanelValue, PANELVALUE_SEQUENCEVALUE, ATTR_DIMMED,		1);
			SetCtrlAttribute(GiPanelValue, PANELVALUE_RANGEVALUE, 	ATTR_DIMMED,		1);
		}

		if (itemValue == 4 || itemValue == 10)
		{

			SetCtrlAttribute(GiPanelValue, PANELVALUE_ONEVALUE, ATTR_DIMMED,			1);
			SetCtrlAttribute(GiPanelValue, PANELVALUE_SEQUENCEVALUE, ATTR_DIMMED,		1);
			SetCtrlAttribute(GiPanelValue, PANELVALUE_RANGEVALUE, 	ATTR_DIMMED,		0);
		}

		if (itemValue == 1)
		{

			SetCtrlAttribute(GiPanelValue, PANELVALUE_ONEVALUE, ATTR_DIMMED,			0);
			SetCtrlAttribute(GiPanelValue, PANELVALUE_SEQUENCEVALUE, ATTR_DIMMED,		0);
			SetCtrlAttribute(GiPanelValue, PANELVALUE_RANGEVALUE, 	ATTR_DIMMED,		1);
		}


		dimmedNumericKeypadForExp(0);
		currentTxtBox = 11;
	}

	if (event == EVENT_KEYPRESS)
	{
		MessagePopup ("Message", "Please use the numeric keypad to enter your value.");
	}  
	
	return 0;
}

int CVICALLBACK TolTxt_Callback (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	if (event == EVENT_LEFT_CLICK)
	{
		dimmedNumericKeypadForExp(0);
		currentTxtBox = 12;
	}
	
	if (event == EVENT_KEYPRESS)
	{
		MessagePopup ("Message", "Please use the numeric keypad to enter your value.");
	}  
	
	return 0;
}


int CVICALLBACK TolPerTxt_Callback (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	if (event == EVENT_LEFT_CLICK)
	{
		dimmedNumericKeypadForExp(0);
		currentTxtBox = 13;
	}
	
	if (event == EVENT_KEYPRESS)
	{
		MessagePopup ("Message", "Please use the numeric keypad to enter your value.");
	}  
	
	return 0;
}

int CVICALLBACK FunctionCodeTxt_Callback (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	if (event == EVENT_LEFT_CLICK)
	{
		dimmedNumericKeypadForExp(0);
	
		currentTxtBox = 14;
	}
	
	if (event == EVENT_KEYPRESS)
	{
		MessagePopup ("Message", "Please use the numeric keypad to enter your value.");
	}  
	
	return 0;
}   



//*************************************************************************** 



// DEBUT ALGO
//***************************************************************************
//int ModesLoadPanel(void *FonctionRetourIhm,
//					char *sSousRepertoireEcran,
//					int iPanelPere,
//					int iUnitVitesse,
//					int iUnitPression)
//***************************************************************************
//    FonctionRetourIhm			: Fonction appelée quand on quitte l'Ihm
//    sSousRepertoireEcran		: Sous répertoire des écrans
//    iPanelPere				: Handle du panel pere
//	  stConfig, *ptStConfig		: Pointeur vers les paramètres de configuration
//	  iUnitVitesse				: Unité de vitesse choisie au lancment de l'application
//	  iUnitPression				: Unité de pression choisie au lancment de l'application
//
//  - Chargement du panel
//
//  - 1 si problème détecté, 0 sinon
//***************************************************************************
// FIN ALGO
int ModesLoadPanel(	void *FonctionRetourIhm,
					char *sSousRepertoireEcran,
					int iPanelPere,
					stConfig *ptStConfig,
					int iUnitVitesse,
					int iUnitPression)
{
	static char sFileNameEcran[MAX_PATHNAME_LEN];
	static char sProjectDir[MAX_PATHNAME_LEN];
	int iError;
	Point cell;

	// Initialisations
	SetGbQuitter(0);

	//--------------------------
	//--------------------------

	strcpy(GsPathResult, "");
	strcpy(GsPathConfigFile, "");
	//DisplayCurrentCycle(GiPanel, GsPathConfigFile);


	// Copie du pointeur vers les paramètres de configuration
	SetGptstConfig(*ptStConfig);

	// Initialisation du nombre d'éléments du tableau des vitesses
	GstTabVitesse.iNbElmts 		= 1;
	GstTabVitesse.dVit[0] 		= 0.0;
	GstTabVitesse.dDuree[0]		= 1.0;
	// Initialisation du nombre d'éléments du tableau des pressions
	GstTabPression.iNbElmts		= 1;
	GstTabPression.dPress[0] 	= 0.0;
	GstTabPression.dDuree[0]	= 1.0;

	// Initialisation des variables globales
	GiPanelPere		= iPanelPere;
	SetGiUnitVitesse(iUnitVitesse);
	SetGiUnitPression(iUnitPression);
	GdCoeffAG=ptStConfig->dCoeffAG;
	// Répertoire du projet
	GetProjectDir (sProjectDir);

	// Formation du chemin vers
	sprintf(sFileNameEcran,"%s\\%s\\%s", sProjectDir, sSousRepertoireEcran, sNOM_IHM_MODES);

	// Chargement du Panel principal
	GiPanel = LoadPanel (GiPanelPere, sFileNameEcran, PANEL_MODE);
	if (GiPanel<0)
	{
		MessagePopup(sFileNameEcran, GetUILErrorString (GiPanel));
		return(1);
	}

	// Chargement du panel d'ajout de lignes
	GiPopupAdd = LoadPanel (GiPanel, sFileNameEcran, POPUP_ADD);
	if (GiPopupAdd<0)
	{
		MessagePopup(sFileNameEcran, GetUILErrorString (GiPopupAdd));
		return(1);
	}

	// Chargement du panel de détail des status
	GiStatusPanel = LoadPanel (0, sFileNameEcran, PANEL_STAT);
	if (GiStatusPanel<0)
	{
		MessagePopup(sFileNameEcran, GetUILErrorString (GiStatusPanel));
		return(1);
	}

	//modif carolina
	// Chargement du panel graph
	GiGraphPanel = LoadPanel (0, sFileNameEcran, PANELGRAPH);
	if (GiGraphPanel<0)
	{
		MessagePopup(sFileNameEcran, GetUILErrorString (GiGraphPanel));
		return(1);
	}

	GiExpectedResultsPanel = LoadPanel (0, sFileNameEcran, EXPRESULTS);
	if (GiExpectedResultsPanel<0)
	{
		MessagePopup(sFileNameEcran, GetUILErrorString (GiExpectedResultsPanel));
		return(1);
	}




	GiPanelValue = LoadPanel (0, sFileNameEcran, PANELVALUE);
	if (GiPanelValue<0)
	{
		MessagePopup(sFileNameEcran, GetUILErrorString (GiPanelValue));
		return(1);
	}

	// Chargement du popup menu (gestion des grilles de données)
	GiPopupMenuHandle = LoadMenuBar (0, sFileNameEcran, GST_TAB);
	GiPopupMenuRunHandle  = LoadMenuBar (0, sFileNameEcran, GST_RUN);
	GiPopupMenuScriptHandle  = LoadMenuBar (0, sFileNameEcran, GST_SCRIPT);
	GiMenuExpResultsHandle = LoadMenuBar (0, sFileNameEcran, MENUEXP);  //modif carolina

	// Traduction de l'IHM
	iError = GstFilesTraductIhmModes(GiPanel, GiPopupMenuHandle, GiStatusPanel, GetGiUnitVitesse(), GetGiUnitPression());

	// Fixe les limites des tableaux de vitesse et de pression
	iError = GstTablesSetLim(GiPanel, PANEL_MODE_TABLE_VITESSES, PANEL_MODE_TABLE_PRESSION, GetGiUnitVitesse(),
							 GetGiUnitPression(), GetPointerToGptstConfig());
	ReleasePointerToGptstConfig();

	// Fixe les limites des saisies manuelles de vitesse et pression
	iError = SetLimSaisiesMan(GiPanel, PANEL_MODE_SAISIE_VITESSE_MANU, PANEL_MODE_SAISIE_PRESSION_MANU, PANEL_MODE_SAISIE_DUREE_VIT_MANU,
							  PANEL_MODE_SAISIE_DUREE_PRES_MAN, GetGiUnitVitesse(), GetGiUnitPression(), GetPointerToGptstConfig());
	ReleasePointerToGptstConfig();

	// Supression des données du tableau des vitesses
	DeleteTableRows (GiPanel, PANEL_MODE_TABLE_VITESSES, 1, 1);
	// Supression des données du tableau des pressions
	DeleteTableRows (GiPanel, PANEL_MODE_TABLE_PRESSION, 1, 1);
	// Ajout d'une ligne dans le tableau des vitesses
	InsertTableRows (GiPanel, PANEL_MODE_TABLE_VITESSES, 1, 1, VAL_USE_MASTER_CELL_TYPE);
	// Ajout d'une ligne dans le tableau des pressions
	InsertTableRows (GiPanel, PANEL_MODE_TABLE_PRESSION, 1, 1, VAL_USE_MASTER_CELL_TYPE);

	cell.x = 2;
	cell.y = 1;
	SetTableCellVal (GiPanel, PANEL_MODE_TABLE_VITESSES, cell, 1.0);
	SetTableCellVal (GiPanel, PANEL_MODE_TABLE_PRESSION, cell, 1.0);

	// Mémorisation de la fonction de retour Ihm
	GFonctionRetourIhm=FonctionRetourIhm;

	return 0;
}


// Quitte le panel d'ajout de script
int CVICALLBACK quitCallback (int panel, int control, int event,
							  void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{
		case EVENT_COMMIT:
			if ( color == 0 && numRow !=0)
			{
			//couleurSelection(numRow,PANEL_MODE_SCRIPTS_DETAILS,GiPanel,VAL_WHITE); 
			couleurLigne(numRow, GiPanel);
			}
			HidePanel(GiPopupAdd2);
		//	couleurSelection(numRow,PANEL_MODE_SCRIPTS_DETAILS, panel,VAL_WHITE )  ;
			//ExcelRpt_WorkbookClose (workbookHandledata, 0);
			//ExcelRpt_ApplicationQuit (applicationHandledata);
			//CA_DiscardObjHandle(applicationHandledata);

			// fermeture d'excel
			ExcelRpt_WorkbookClose (workbookHandle5, 0);
			//ExcelRpt_ApplicationQuit (applicationHandle5);
			//CA_DiscardObjHandle(applicationHandle5);

			break;
	}
	return 0;
}

// Charge un script
/*int CVICALLBACK boutonLoad (int panel, int control, int event,
							void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{

		char pathName[MAX_PATHNAME_LEN];
		int boucle;
		Point cell;

		int EndBoucle=0;
		int iCellValue=0;
		//double dCellValue;
		char sCellValue[200]="";
		char range[30];
		//char st[30];

		case EVENT_COMMIT:

			FileSelectPopup ("D:\\Script files", "*.xlsx", "*.xlsx", "Load script file", VAL_LOAD_BUTTON, 0, 1, 1, 1, pathName);
		// Ouverture d'excel pour charger le fichier
			//ExcelRpt_ApplicationNew(VTRUE,&applicationHandle3);
			//ExcelRpt_WorkbookOpen (applicationHandle3, pathName, &workbookHandle3);
			ExcelRpt_GetWorksheetFromIndex(workbookHandle3, 1, &worksheetHandle3);

		//	ExcelRpt_GetWorksheetAttribute (worksheetHandle3, ER_WS_ATTR_USED_RANGE, st);

			ExcelRpt_GetCellValue (worksheetHandle3, "I1", CAVT_INT,&EndBoucle);

			*pointeurval= EndBoucle-1;

			DeleteTableRows (panel, PANEL_MODE_SCRIPTS_DETAILS, 1, -1);

			// charge chaque lignes du tableau
			for( boucle=1;boucle <= EndBoucle-1;boucle++)
			{

			//	InsertTableRows (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, -1, 1, VAL_USE_MASTER_CELL_TYPE);
				InsertTableRows (panel, PANEL_MODE_SCRIPTS_DETAILS, -1, 1, VAL_USE_MASTER_CELL_TYPE);


				cell.y =boucle;
				cell.x =1;
				sprintf(range,"A%d",boucle+1);
				ExcelRpt_GetCellValue (worksheetHandle3, range, CAVT_CSTRING,sCellValue);
				SetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
			//	SetTableCellAttribute (panel, PANEL_MODE_SCRIPTS_DETAILS, cell, VAL_INTEGER, iCellValue);


				cell.x =2;
				sprintf(range,"B%d",boucle+1);
				ExcelRpt_GetCellValue (worksheetHandle3,range , CAVT_CSTRING,sCellValue);
				SetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);

				cell.x =3;
				sprintf(range,"C%d",boucle+1);
				ExcelRpt_GetCellValue (worksheetHandle3,range , CAVT_CSTRING,sCellValue);
				SetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);

				cell.x =4;
				sprintf(range,"D%d",boucle+1);
				ExcelRpt_GetCellValue (worksheetHandle3,range , CAVT_CSTRING,sCellValue);
				SetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);

				cell.x =5;
				sprintf(range,"E%d",boucle+1);
				ExcelRpt_GetCellValue (worksheetHandle3,range , CAVT_CSTRING,sCellValue);
				SetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);

				cell.x =6;
				sprintf(range,"F%d",boucle+1);
				ExcelRpt_GetCellValue (worksheetHandle3,range , CAVT_CSTRING,sCellValue);
				SetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);

				cell.x =7;
				sprintf(range,"G%d",boucle+1);
				ExcelRpt_GetCellValue (worksheetHandle3,range , CAVT_CSTRING,sCellValue);
				SetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);

				cell.x =8;
				sprintf(range,"H%d",boucle+1);
				ExcelRpt_GetCellValue (worksheetHandle3,range , CAVT_CSTRING,sCellValue);
				SetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);

				}

				cell.x =1;
				sprintf(range,"A%d",boucle+1);
				ExcelRpt_GetCellValue (worksheetHandle3, range, CAVT_INT,&iCellValue);
				SetCtrlVal(panel,PANEL_MODE_ENDTIME,iCellValue);


			// Fermeture excel
				//ExcelRpt_WorkbookClose (workbookHandle3, 0);
				//ExcelRpt_ApplicationQuit (applicationHandle3);
				//CA_DiscardObjHandle(applicationHandle3);
			break;
	}
	return 0;
} */

//*****************************************************************************************
//Modif - Carolina 19/02/2019
//Nettoyage du code, Label in case 8, ergonomic
//
//*****************************************************************************************
int CVICALLBACK select_function (int panel, int control, int event,
								 void *callbackData, int eventData1, int eventData2)
{
	int itemValue=0;
	//int iVal=0;
	//char sVal[20]="";
	//char sValInterface[20]="";
	//int toggleButtonVal=1;
	char stringNbframe[50] = ""; //MODIF Maxime PAGES 11/06/2020

	switch (event)
	{
			//**********************************
			//MODIF  Maxime PAGES 11/06/2020
			// On initialise ici le LFDNBFRAME
		case EVENT_COMMIT:

			
			
			for (int i=1; i<256; i++)
			{
				sprintf(stringNbframe, "%d",i);  // on convertir le int en string
				InsertListItem (GiPopupAdd2,PANEL_ADD_LFDNBFRAME,i,stringNbframe,stringNbframe);
			}
			//**********************************

		case EVENT_VAL_CHANGED:
			GetCtrlVal(panel,PANEL_ADD_FUNCTION_SELECT, &itemValue);

			// remise a zero des indicateur

			SetCtrlVal(GiPopupAdd2,PANEL_ADD_TIME,				0.0);
			SetCtrlVal(GiPopupAdd2,PANEL_ADD_LFPOWER,			0);
			SetCtrlVal(GiPopupAdd2,PANEL_ADD_PRESSURE,			"");
			SetCtrlVal(GiPopupAdd2,PANEL_ADD_ACCELERATION,		"");
			SetCtrlVal(GiPopupAdd2,PANEL_ADD_DURATION,			0.0);
			SetCtrlVal(GiPopupAdd2,PANEL_ADD_LFDNAME,			0);
			SetCtrlVal(GiPopupAdd2,PANEL_ADD_LFDNBFRAME,			0); //AJOUT Maxime PAGES - 10/06/2020
			//SetCtrlVal(GiPopupAdd2,PANEL_ADD_CHECKCONTINUOUS, 	0);
			//SetCtrlVal(GiPopupAdd2,PANEL_ADD_NUMBER,			0);
			SetCtrlVal(GiPopupAdd2,PANEL_ADD_INTERFRAME,		"");
		//	SetCtrlVal(GiPopupAdd2,PANEL_ADD_WUID,				8);  //MODIF Maxime PAGES - 15/06/2020   E104
			SetCtrlVal(GiPopupAdd2,PANEL_ADD_LABEL_STRING,		"");
			SetCtrlVal(GiPopupAdd2,PANEL_ADD_TIME2,				"");
			SetCtrlVal(GiPopupAdd2,PANEL_ADD_DURATION2,			"");
			SetCtrlVal(GiPopupAdd2,PANEL_ADD_ACCELERATION,		"");
			SetCtrlVal(GiPopupAdd2,PANEL_ADD_PRESSURE,			"");
			SetCtrlVal(GiPopupAdd2,PANEL_ADD_LFPOWER,			"");
			//SetCtrlVal(GiPopupAdd2,PANEL_ADD_NUMBER,			"");
			SetCtrlVal(GiPopupAdd2,PANEL_ADD_INTERFRAME,		"");
			SetCtrlVal(GiPopupAdd2,PANEL_ADD_WUID_VALUE,		"");

			//SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_CHECKCONTINUOUS,ATTR_DIMMED,1);

			insert_parameter=0;

			if (insert_parameter==0)
			{

				SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT1,			ATTR_DIMMED, 1); //from time
				SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT2,			ATTR_DIMMED, 1); //from duration
				dimmedNumericKeypadForScript(1);

			}

			break;
	}

	switch (itemValue)
	{
		case 0:  //Initial screen

			break;

		case 1:   //SetA
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_TIME2,ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_DURATION2,ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT_PARAMETER,ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFPOWER,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_PRESSURE,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_ACCELERATION,ATTR_DIMMED,0); //button
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT_PARAMETER2,ATTR_DIMMED,0);	  //button
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFDNAME,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFDNBFRAME,ATTR_DIMMED,1); //AJOUT Maxime PAGES - 10/06/2020
			//SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_CHECKCONTINUOUS,ATTR_DIMMED,1);
			//SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_NUMBER,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INTERFRAME,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LABEL_STRING,ATTR_DIMMED, 1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID_VALUE,ATTR_DIMMED,1);

			insert_parameter=1;

			break;

		case 2:	   //SetP
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_TIME2,ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_DURATION2,ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT_PARAMETER,ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFPOWER,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_PRESSURE,ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_ACCELERATION,ATTR_DIMMED,1); //button
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT_PARAMETER2,ATTR_DIMMED,0);	  //button
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFDNAME,ATTR_DIMMED,1);
			//SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_CHECKCONTINUOUS,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFDNBFRAME,ATTR_DIMMED,1); //AJOUT Maxime PAGES - 10/06/2020
			//SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_NUMBER,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INTERFRAME,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LABEL_STRING,ATTR_DIMMED, 1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID_VALUE,ATTR_DIMMED,1);

			insert_parameter=1;

			break;

		case 3:   //SendLFD
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT_PARAMETER,		ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFPOWER,					ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_PRESSURE,				ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_ACCELERATION,			ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT_PARAMETER2,		ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFDNAME,					ATTR_DIMMED,0);
			//SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_CHECKCONTINUOUS,			ATTR_DIMMED,0);
			//SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_NUMBER,					ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFDNBFRAME,				ATTR_DIMMED,0); //AJOUT Maxime PAGES - 10/06/2020
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INTERFRAME,				ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID,					ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LABEL_STRING,			ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID_VALUE,				ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_TIME2,					ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_DURATION2,				ATTR_DIMMED,1);



			//SetCtrlVal (GiPopupAdd2, PANEL_ADD_CHECKCONTINUOUS, toggleButtonVal);

			insert_parameter=1;

			break;

		case 4: //SendLFCw
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT_PARAMETER,		ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFPOWER,					ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_PRESSURE,				ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_ACCELERATION,			ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT_PARAMETER2,		ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFDNAME,					ATTR_DIMMED,0);
			//SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_CHECKCONTINUOUS,			ATTR_DIMMED,0);
			//SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_NUMBER,					ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFDNBFRAME,					ATTR_DIMMED,0); //AJOUT Maxime PAGES - 10/06/2020
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INTERFRAME,				ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID,					ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_TIME2,					ATTR_DIMMED,0); //time
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_DURATION2,				ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LABEL_STRING,			ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID_VALUE,				ATTR_DIMMED,0);



			//SetCtrlVal (GiPopupAdd2, PANEL_ADD_CHECKCONTINUOUS, toggleButtonVal);

			insert_parameter=1;

			break;

		case 5:	   //LFPower
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT_PARAMETER,ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFPOWER,ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_PRESSURE,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_ACCELERATION,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT_PARAMETER2,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFDNAME,ATTR_DIMMED,1);
			//SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_CHECKCONTINUOUS,ATTR_DIMMED,1);
			//SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_NUMBER,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFDNBFRAME,ATTR_DIMMED,1); //AJOUT Maxime PAGES - 10/06/2020
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INTERFRAME,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_TIME2,ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_DURATION2,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LABEL_STRING,ATTR_DIMMED, 1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID_VALUE,ATTR_DIMMED,1);

			insert_parameter=1;

			break;

		case 6:		//StopLFD
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT_PARAMETER,ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFPOWER,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_PRESSURE,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_ACCELERATION,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT_PARAMETER2,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFDNAME,ATTR_DIMMED,1);
			//SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_CHECKCONTINUOUS,ATTR_DIMMED,1);
			//SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_NUMBER,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFDNBFRAME,ATTR_DIMMED,1); //AJOUT Maxime PAGES - 10/06/2020
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INTERFRAME,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_TIME2,ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_DURATION2,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LABEL_STRING,ATTR_DIMMED, 1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID_VALUE,ATTR_DIMMED,1);

			insert_parameter=1;

			break;

		case 7:   //StopLFCw
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT_PARAMETER,		ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFPOWER,					ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_PRESSURE,				ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_ACCELERATION,			ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT_PARAMETER2,		ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFDNAME,					ATTR_DIMMED,1);
			//SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_CHECKCONTINUOUS,			ATTR_DIMMED,1);
			//SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_NUMBER,					ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFDNBFRAME,					ATTR_DIMMED,1); //AJOUT Maxime PAGES - 10/06/2020
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INTERFRAME,				ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID,					ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_TIME2,					ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_DURATION2,				ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LABEL_STRING,			ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID_VALUE,				ATTR_DIMMED,1);

			insert_parameter=1;

			break;

		case 8:   //Label
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT_PARAMETER,ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFPOWER,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_PRESSURE,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_ACCELERATION,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT_PARAMETER2,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFDNAME,ATTR_DIMMED,1);
			//SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_CHECKCONTINUOUS,ATTR_DIMMED,1);
			//SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_NUMBER,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFDNBFRAME,ATTR_DIMMED,1); //AJOUT Maxime PAGES - 10/06/2020
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INTERFRAME,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_TIME2,ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_DURATION2,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LABEL_STRING,ATTR_DIMMED, 0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID_VALUE,ATTR_DIMMED,1);

			insert_parameter=1;

			break;
	}
	return 0;
}

//*******************************************
//MODDIF MaximePAGES - 11/06/2020

//   Check Bouton Continuous
/*
int CVICALLBACK checkcontinuous (int panel, int control, int event,
								 void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{
			int toggleButtonVal;
			int iVal=0;


		case EVENT_COMMIT:

				 toggleButtonVal=0;

				GetCtrlVal (panel, PANEL_ADD_CHECKCONTINUOUS, &toggleButtonVal);
				if(toggleButtonVal==1)
				{
					SetCtrlAttribute(panel,PANEL_ADD_NUMBER,ATTR_DIMMED,1);
					SetCtrlVal(panel,PANEL_ADD_NUMBER,iVal);
				}
				else
				{
					SetCtrlAttribute(panel,PANEL_ADD_NUMBER,ATTR_DIMMED,0);
				}


			break;
	}
	return 0;
}  */


//*****************************************************************************************
// Ajoute au tableau des script les différents paramètres
// Modif - Carolina 02/2019
// Code cleanup (not finished), Put Label into the Test Script , interface more ergonomic
// Put value (alphanumeric) on the label
// code for add line on the expected results
//*****************************************************************************************
int CVICALLBACK addCallback (int panel, int control, int event,
							 void *callbackData, int eventData1, int eventData2)
{
	//int toggleButtonVal=0;
	//int lfdNameVal=0;
	Point cell;
	Rect rect;
	cell.x=1;
	
	
	char nameMLF[100]="";
	int find = 0;

	int delete = 0;
	/*char wuid1[100]="ID1";
	char wuid2[100]="IDDIAG";
	char wuid3[100]="IDSWIdent1";
	char wuid4[100]="ID4";
	char wuid5[100]="ID5";
	char wuid6[100]="ID6"; */
	//int iwuid=0;


	char pressure[100] = "";
	char pressure2[100]	="";

	char acceleration[100] = "";
	char acceleration2[100]	= "";

	char labelname[100]	= "";
	char labelname2[100] = "";

	char lfpower[100]="";
	char lfpower2[100]="";

	//char number[100]="";
	char interframe[100]="";
	int itemValue=0;
	char labelWUID[100];
	int indexWUID = 0;
	//char itemSelected[100] = "";


	//int itemMLF=10;
	char Formula[500]="";
	char Formula2[500]="";
	char timecell[500] = "";

	int valueBox=0;
	char valueNbLF[100]= "";  // AJOUT Maxime PAGES - 10/06/2020

	char condition1[20]="Pre";
	char condition2[20]="Script";
	char condition3[20]="Post";

	// Nom des fonctions
	char function[]="NoFunction";
	char function1[]="SetA";
	char function2[]="SetP";
	char function3[]="SendLFD";
	char function4[]="SendLFCw";
	char function5[]="LFPower";
	char function6[]="StopLFD";
	char function7[]="StopLFCw";
	char function8[]="Label";  //it has time /value atm pressure

	/*char mlf[]="0";
	char mlf1[]="MLF1";
	char mlf2[]="MLF2";
	char mlf3[]="MLF3";*/
	//char *test;
	switch (event)
	{
		case EVENT_COMMIT:
			
			 
			//MARVYN 23/06/2021
			//couleurSelection(numRow,PANEL_MODE_SCRIPTS_DETAILS,GiPanel,VAL_WHITE);
			if (insertSteps == 1 || modifySteps ==1)
			{
				
				couleurLigne(numRow, GiPanel);
			}
			color = 1;
			//***********
			if(insertSteps == 0 && modifySteps == 0)
			{
			InsertTableRows (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, -1, 1, VAL_USE_MASTER_CELL_TYPE);
			*pointeurval= *pointeurval+1;
			//printf("premier if \n");
			cell.y=*pointeurval;
			}
			else if (insertSteps ==1)
			{
			
				InsertTableRows (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, numRow, 1, VAL_USE_MASTER_CELL_TYPE);
				*pointeurval= *pointeurval+1;
				cell.y = numRow ;
				
			}
			else if (modifySteps == 1)
			{
				
			cell.y = numRow ; 
		//	InsertTableRows (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, numRow, 1, VAL_USE_MASTER_CELL_TYPE);
		//	DeleteTableRows (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, numRow+1, tableHeight);
			itemValue= command ;
			}
			else cell.y =1;

			// Affiche des paramètre depuis le panel d'ajout vers le tableau
			
			cell.x=1;
			
			GetCtrlVal(panel,PANEL_ADD_TIME2,Formula);
			strcpy(timecell, Formula);
			if(strcmp(timecell, "") == 0)
			{
				MessagePopup ("Warning", "Time cell can not be empty");
			}
			SetTableCellVal (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, cell, Formula);

			cell.x=2;
			GetCtrlVal(panel,PANEL_ADD_DURATION2,Formula2);
			SetTableCellVal (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, cell, Formula2);

			cell.x=3;
			if (modifySteps != 1) 
			{
			GetCtrlVal(panel,PANEL_ADD_FUNCTION_SELECT, &itemValue);
			}
			
			switch (itemValue)
			{
				case 0:
					//printf("function = %s\n",function);
					SetTableCellVal (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, cell, function);
				//	SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, function);

					break;
				case 1:		  //acceleration
					SetTableCellVal (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, cell, function1);

					cell.x=4;
					GetCtrlVal(panel,PANEL_ADD_ACCELERATION, acceleration);
					sprintf(acceleration2,"%s[]",acceleration);
					SetTableCellVal (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, cell, acceleration2);

					break;
				case 2:		   //pressure
					SetTableCellVal (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, cell, function2);

					cell.x=4;
					GetCtrlVal(panel,PANEL_ADD_PRESSURE, pressure);
					sprintf(pressure2,"%s[]",pressure);
					SetTableCellVal (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, cell, pressure2);

					break;
				case 3:
					SetTableCellVal (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, cell, function3);

					cell.x=4;
					GetCtrlVal(panel,PANEL_ADD_LFDNAME, nameMLF);
					SetTableCellVal (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, cell, nameMLF);

					cell.x=5;


					GetCtrlVal(panel,PANEL_ADD_LFDNBFRAME, valueNbLF);
					if (!(strcmp(valueNbLF,"0")))  //on regarde si on est dans le cas Continuous
					{
						SetTableCellVal (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, cell, "Continuous");
					}
					else
					{
						
						SetTableCellVal (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, cell, valueNbLF);
					}

					//*****************************************
					cell.x=6;
					//Modif MaximePAGES 13/08/2020 -  WU ID modification
					GetCtrlIndex(panel,PANEL_ADD_WUID, &indexWUID);
					GetLabelFromIndex (panel,PANEL_ADD_WUID, indexWUID,labelWUID);
					
					if (strcmp(labelWUID,"ID ind") == 0) 
					{
						SetTableCellVal (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, cell, "");
					}
					else 
					{
						SetTableCellVal (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, cell, labelWUID);	
					}
						
					//GetCtrlVal(panel,PANEL_ADD_WUID_VALUE, value_ind);
					//SetTableCellVal (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, cell, value_ind);

					cell.x=7;
					GetCtrlVal(panel,PANEL_ADD_INTERFRAME, interframe);
					SetTableCellVal (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, cell, interframe);

					break; //final case 3
				case 4:
					SetTableCellVal (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, cell, function4);

					cell.x=4;
					GetCtrlVal(panel,PANEL_ADD_LFDNAME, nameMLF);
					SetTableCellVal (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, cell, nameMLF);

					cell.x=5;

					//*****************************************
					//MODIF MaximePAGES - 10/06/2020


					GetCtrlVal(panel,PANEL_ADD_LFDNBFRAME, valueNbLF);
					if (!(strcmp(valueNbLF,"0")))  //on regarde si on est dans le cas Continuous
					{
						SetTableCellVal (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, cell, "Continuous");
					}
					else
					{
						SetTableCellVal (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, cell, valueNbLF);
					}

					//************************************

					cell.x=6;
					//Modif MaximePAGES 13/08/2020 -  WU ID modification
					GetCtrlIndex(panel,PANEL_ADD_WUID, &indexWUID);
					GetLabelFromIndex (panel,PANEL_ADD_WUID, indexWUID,labelWUID);
					
					if (strcmp(labelWUID,"ID ind") == 0) 
					{
						SetTableCellVal (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, cell, "");
					}
					else 
					{
						SetTableCellVal (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, cell, labelWUID);	
					}

					
					//GetCtrlVal(panel,PANEL_ADD_WUID_VALUE, value_ind);
					//SetTableCellVal (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, cell, value_ind);

					break;
				case 5:
					SetTableCellVal (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, cell, function5);

					cell.x=4;
					GetCtrlVal(panel,PANEL_ADD_LFPOWER, lfpower);
					sprintf(lfpower2,"%s[]",lfpower);
					SetTableCellVal (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, cell, lfpower2);

					break;
				case 6:
					SetTableCellVal (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, cell, function6);

					break;
				case 7:
					SetTableCellVal (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, cell, function7);

					break;
				case 8: //Put function8 in the Panel Test Script
					SetTableCellVal (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, cell, function8);

					cell.x=3;
					GetCtrlVal(panel,PANEL_ADD_LABEL_STRING, labelname);
					sprintf(labelname2,"%s",labelname);
					
				//MARVYN 23/07/2021	
					//if (strcmp(labelsInsert[0],"") != 0 )
				//	{
					//printf("2tab%d = %s\n",indexLab,labelsInsert[0]);  
				//find = labelAlreadyExists(labelname); 
					find = verifLabel(labelname);
				//	}
					//
						if (find==0)
						{
							SetTableCellVal (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, cell, labelname);
							//strcpy(labelsInsert[indexLab],labelname);
							//printf("tab%d = %s\n",indexLab,labelsInsert[0]);
							//indexLab++;
							find =0;
						}
						else
						{
							if (modifySteps == 1 && strcmp(labelname,labelModif)==0)
							{
							SetTableCellVal (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, cell, labelname);
							}	
							else
							{
							MessagePopup("Warning", "Label already exists, please choose another name!");
							SetTableCellVal (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, cell, "");
								if ( insertSteps == 1 )
								{
								DeleteTableRows (GiPanel, PANEL_MODE_SCRIPTS_DETAILS,numRow,1); 
								}
								else
								{
								DeleteTableRows (GiPanel, PANEL_MODE_SCRIPTS_DETAILS,*pointeurval,1);
								*pointeurval = *pointeurval - 1;
								}
								delete = 1;
							}
						}
				//Modif MaximePAGES 03/08/2020 // Modif MarvynPANNETIER 17/06/2021 : label is not a list 
				//	InsertListItem (GiExpectedResultsPanel,EXPRESULTS_LABEL1  , -1, labelname2, 0);
					break;
			}
			cell.x=8;

			if (delete ==0)
			{
			GetCtrlVal(panel,PANEL_ADD_BOX_PRE,&valueBox);
			if(valueBox==1)
			{
				SetTableCellVal (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, cell, condition1);
			}

			GetCtrlVal(panel,PANEL_ADD_BOX_SCRIPT,&valueBox);
			if(valueBox==1)
			{
				SetTableCellVal (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, cell, condition2);
			}

			GetCtrlVal(panel,PANEL_ADD_BOX_POST,&valueBox);
			if(valueBox==1)
			{
				SetTableCellVal (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, cell, condition3);
			}
			}
			if (modifySteps == 1 || insertSteps ==1)
			{
				couleurLigne(numRow, GiPanel);
			}
			else 
			{
			couleurLigne(*pointeurval, GiPanel);
			} 


			break;   //EVENT_COMMIT:
	}

	return 0;
}


// Affecte la valeur et l'unité au paramètre choisi
int CVICALLBACK parametersCallback (int panel, int control, int event,
									void *callbackData, int eventData1, int eventData2)
{
	int itemValue=0;
	char valueParameter[100];
	char unitParameter[100];
	char Formula[500]="";

	switch (event)
	{
		case EVENT_VAL_CHANGED:
			
			GetCtrlVal(panel,PANEL_ADD_PARAMETERS, &itemValue);

			if( itemValue == 0)
			{
				 SetCtrlVal(panel,PANEL_ADD_VALUE,""); 
				 SetCtrlVal(panel,PANEL_ADD_UNIT,"") ;
				
			}
			else
			{

				strcpy(valueParameter,myParameterTab[itemValue-1].value);
				SetCtrlVal(panel,PANEL_ADD_VALUE,valueParameter);

				strcpy(unitParameter,myParameterTab[itemValue-1].unit) ;
				SetCtrlVal(panel,PANEL_ADD_UNIT,unitParameter);

				GetCtrlVal(panel,PANEL_ADD_FORMULA, Formula);
				strcat(Formula, " ");
				strcat(Formula, myParameterTab[itemValue-1].name);
				SetCtrlVal(panel,PANEL_ADD_FORMULA, Formula);
			}
			
			break;
	}
	return 0;
}

//*****************************************************************************
//Fin Modif
// Modifications LC

/*int CVICALLBACK FormulaAddCallback (int panel, int control, int event,
									void *callbackData, int eventData1, int eventData2)
{

	char Param[500]="";
	char sValue[500]="";
	char Formula[500]="";
	int iValue=0;
	int ListIndex=0;
	int itemValue=0;

	switch (event)
	{
		case EVENT_COMMIT:

			GetCtrlVal(panel,PANEL_ADD_PARAMETERS, &itemValue);  //menu de parameters

			if( itemValue == 0)
			{
			}
			else
			{

				GetCtrlVal(panel, PANEL_ADD_PARAMETERS, &ListIndex);
				GetLabelFromIndex(panel,PANEL_ADD_PARAMETERS, ListIndex, Param);
				GetCtrlVal(panel,PANEL_ADD_VALUE, sValue);
				iValue = atoi(sValue);

				GetCtrlVal(panel,PANEL_ADD_FORMULA, Formula);
				strcat(Formula, " ");
				strcat(Formula, Param);
				SetCtrlVal(panel,PANEL_ADD_FORMULA, Formula);
			}
			break;
	}
	return 0;
}   */
//***********************************************************************************************
//
//-------------------------------------BUTTONS FORMULE-------------------------------------------
// Bouton pour ajouter parathèse 1
int CVICALLBACK boutonpar1 (int panel, int control, int event,
							void *callbackData, int eventData1, int eventData2)
{
	char Param[500]=" ( ";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:

			GetCtrlVal(panel,PANEL_ADD_FORMULA,Formula);
			strcat(Formula, Param);
			SetCtrlVal(panel,PANEL_ADD_FORMULA,Formula);


			break;
	}
	return 0;
}

// Bouton pour ajouter parathèse 2
int CVICALLBACK boutonpar2 (int panel, int control, int event,
							void *callbackData, int eventData1, int eventData2)
{
	char Param[500]=" ) ";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(panel,PANEL_ADD_FORMULA,Formula);
			strcat(Formula, Param);
			SetCtrlVal(panel,PANEL_ADD_FORMULA,Formula);
			break;
	}
	return 0;
}

// Bouton pour ajouter addition
int CVICALLBACK boutonadd (int panel, int control, int event,
						   void *callbackData, int eventData1, int eventData2)
{
	char Param[500]=" + ";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(panel,PANEL_ADD_FORMULA,Formula);
			strcat(Formula, Param);
			SetCtrlVal(panel,PANEL_ADD_FORMULA,Formula);
			break;
	}
	return 0;
}

// Bouton pour ajouter soustraction
int CVICALLBACK boutonsou (int panel, int control, int event,
						   void *callbackData, int eventData1, int eventData2)
{
	char Param[500]=" - ";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(panel,PANEL_ADD_FORMULA,Formula);
			strcat(Formula, Param);
			SetCtrlVal(panel,PANEL_ADD_FORMULA,Formula);
			break;
	}
	return 0;
}

// Bouton pour ajouter multiplication
int CVICALLBACK boutonmul (int panel, int control, int event,
						   void *callbackData, int eventData1, int eventData2)
{
	char Param[500]=" * ";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(panel,PANEL_ADD_FORMULA,Formula);
			strcat(Formula, Param);
			SetCtrlVal(panel,PANEL_ADD_FORMULA,Formula);
			break;
	}
	return 0;
}

// Bouton pour ajouter division
int CVICALLBACK boutondiv (int panel, int control, int event,
						   void *callbackData, int eventData1, int eventData2)
{
	char Param[500]=" / ";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(panel,PANEL_ADD_FORMULA,Formula);
			strcat(Formula, Param);
			SetCtrlVal(panel,PANEL_ADD_FORMULA,Formula);
			break;
	}
	return 0;
}

//Modif MaximePAGES 27/07/2020 -new IHM parameters  numeric keypad   ****************************

// Bouton pour ajouter dot
int CVICALLBACK boutondot (int panel, int control, int event,
						   void *callbackData, int eventData1, int eventData2)
{
	char Param[500]=".";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(panel,PANEL_ADD_FORMULA,Formula);
			strcat(Formula, Param);
			SetCtrlVal(panel,PANEL_ADD_FORMULA,Formula);
			break;
	}
	return 0;
}

// Bouton pour ajouter 0
int CVICALLBACK bouton0 (int panel, int control, int event,
						 void *callbackData, int eventData1, int eventData2)
{
	char Param[500]="0";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(panel,PANEL_ADD_FORMULA,Formula);
			strcat(Formula, Param);
			SetCtrlVal(panel,PANEL_ADD_FORMULA,Formula);
			break;
	}
	return 0;
}

// Bouton pour ajouter 1
int CVICALLBACK bouton1 (int panel, int control, int event,
						 void *callbackData, int eventData1, int eventData2)
{
	char Param[500]="1";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(panel,PANEL_ADD_FORMULA,Formula);
			strcat(Formula, Param);
			SetCtrlVal(panel,PANEL_ADD_FORMULA,Formula);
			break;
	}
	return 0;
}

// Bouton pour ajouter 2
int CVICALLBACK bouton2 (int panel, int control, int event,
						 void *callbackData, int eventData1, int eventData2)
{
	char Param[500]="2";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(panel,PANEL_ADD_FORMULA,Formula);
			strcat(Formula, Param);
			SetCtrlVal(panel,PANEL_ADD_FORMULA,Formula);
			break;
	}
	return 0;
}

// Bouton pour ajouter 3
int CVICALLBACK bouton3 (int panel, int control, int event,
						 void *callbackData, int eventData1, int eventData2)
{
	char Param[500]="3";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(panel,PANEL_ADD_FORMULA,Formula);
			strcat(Formula, Param);
			SetCtrlVal(panel,PANEL_ADD_FORMULA,Formula);
			break;
	}
	return 0;
}

// Bouton pour ajouter 4
int CVICALLBACK bouton4 (int panel, int control, int event,
						 void *callbackData, int eventData1, int eventData2)
{
	char Param[500]="4";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(panel,PANEL_ADD_FORMULA,Formula);
			strcat(Formula, Param);
			SetCtrlVal(panel,PANEL_ADD_FORMULA,Formula);
			break;
	}
	return 0;
}

// Bouton pour ajouter 5
int CVICALLBACK bouton5 (int panel, int control, int event,
						 void *callbackData, int eventData1, int eventData2)
{
	char Param[500]="5";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(panel,PANEL_ADD_FORMULA,Formula);
			strcat(Formula, Param);
			SetCtrlVal(panel,PANEL_ADD_FORMULA,Formula);
			break;
	}
	return 0;
}

// Bouton pour ajouter 6
int CVICALLBACK bouton6 (int panel, int control, int event,
						 void *callbackData, int eventData1, int eventData2)
{
	char Param[500]="6";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(panel,PANEL_ADD_FORMULA,Formula);
			strcat(Formula, Param);
			SetCtrlVal(panel,PANEL_ADD_FORMULA,Formula);
			break;
	}
	return 0;
}

// Bouton pour ajouter 7
int CVICALLBACK bouton7 (int panel, int control, int event,
						 void *callbackData, int eventData1, int eventData2)
{
	char Param[500]="7";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(panel,PANEL_ADD_FORMULA,Formula);
			strcat(Formula, Param);
			SetCtrlVal(panel,PANEL_ADD_FORMULA,Formula);
			break;
	}
	return 0;
}

// Bouton pour ajouter 8
int CVICALLBACK bouton8 (int panel, int control, int event,
						 void *callbackData, int eventData1, int eventData2)
{
	char Param[500]="8";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(panel,PANEL_ADD_FORMULA,Formula);
			strcat(Formula, Param);
			SetCtrlVal(panel,PANEL_ADD_FORMULA,Formula);
			break;
	}
	return 0;
}

// Bouton pour ajouter 9
int CVICALLBACK bouton9 (int panel, int control, int event,
						 void *callbackData, int eventData1, int eventData2)
{
	char Param[500]="9";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(panel,PANEL_ADD_FORMULA,Formula);
			strcat(Formula, Param);
			SetCtrlVal(panel,PANEL_ADD_FORMULA,Formula);
			break;
	}
	return 0;
}




//********************************************************************************************
//---------------------------------------------------------------------------------------------


//********************************************************************************************
// Calcul la formule et l'ajoute dans l'indicateur time
//Modif - Carolina 02/2019
// Local in the function : ExcelRpt_WorkbookOpen
//*********************************************************************************************
int CVICALLBACK insert1 (int panel, int control, int event,
						 void *callbackData, int eventData1, int eventData2)
{
	char Formula1[500]="";

	switch (event)
	{
		case EVENT_COMMIT:


			GetCtrlVal(panel,PANEL_ADD_FORMULA, Formula1); //prendre la formule
			SetCtrlVal(panel,PANEL_ADD_TIME2,	Formula1);
			//printf("\n\n panel = %d", panel);
			SetCtrlAttribute (panel, PANEL_ADD_PARAMETERS, ATTR_DFLT_VALUE, 0);
			DefaultCtrl (panel, PANEL_ADD_PARAMETERS);

			//Modif MaximePAGES 28/07/2020
			//Value1 = parameterToValue (Formula1);
			Value1 = atof(parameterToValue_Str(Formula1));    //Modif MaximePAGES 05/08/2020

			SetCtrlVal(panel,PANEL_ADD_TIME,Value1);
			SetCtrlVal(panel,PANEL_ADD_FORMULA,"");
			SetCtrlVal(panel,PANEL_ADD_VALUE,"");
			SetCtrlVal(panel,PANEL_ADD_UNIT,"");

			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT1,ATTR_DIMMED, 1); //from time
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT2,ATTR_DIMMED, 1); //from duration
			dimmedNumericKeypadForScript(1);  

			break;
	}
	return 0;
}


int CVICALLBACK insert1bis (int panel, int control, int event,
						 void *callbackData, int eventData1, int eventData2)
{
	char Formula1[500]="";

	switch (event)
	{
		case EVENT_COMMIT:


			GetCtrlVal(panel,PANEL_ADD_FORMULA, Formula1); //prendre la formule
			//	printf("\n\n panel = %d", panel); 
			//Modif MaximePAGES 10/08/2020 **************************************  
			switch(currentTxtBox)
			{
				case 1: //Time text box
					SetCtrlVal(panel,PANEL_ADD_TIME2,	Formula1);
					break;

				case 2: //Duration text box 
					SetCtrlVal(panel,PANEL_ADD_DURATION2,	Formula1);
					break;

				case 3: //Pressure text box 
					SetCtrlVal(panel,PANEL_ADD_PRESSURE,	Formula1);
					break;

				case 4: //Acc text box 
					SetCtrlVal(panel,PANEL_ADD_ACCELERATION,	Formula1);
					break;
					
				case 5:  //Interframe text box   
					SetCtrlVal(panel,PANEL_ADD_INTERFRAME,	Formula1);
					break;
					
				case 6:  //LFpower text box
					SetCtrlVal(panel,PANEL_ADD_LFPOWER,	Formula1);
					break;
					

					// operator doesn't match any case
				default:
					printf("Error! operator is not correct");
			}

			//******************************************************************** 

			SetCtrlAttribute (panel, PANEL_ADD_PARAMETERS, ATTR_DFLT_VALUE, 0);
			DefaultCtrl (panel, PANEL_ADD_PARAMETERS);

			//Modif MaximePAGES 28/07/2020
			//Value1 = parameterToValue (Formula1);
			Value1 = atof(parameterToValue_Str(Formula1));    //Modif MaximePAGES 05/08/2020

			
			SetCtrlVal(panel,PANEL_ADD_RESULT,Value1); 
			
			SetCtrlVal(panel,PANEL_ADD_FORMULA,"");
			SetCtrlVal(panel,PANEL_ADD_VALUE,"");
			SetCtrlVal(panel,PANEL_ADD_UNIT,"");

			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT1,ATTR_DIMMED, 1); //from time
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT2,ATTR_DIMMED, 1); //from duration
			dimmedNumericKeypadForScript(1);  

			break;
	}
	return 0;
}


//********************************************************************************************
// Calcul la formule et l'ajoute dans l'indicateur duration
//Modif - Carolina 02/2019
// Local in the function : ExcelRpt_WorkbookOpen
//*********************************************************************************************
int CVICALLBACK insert2 (int panel, int control, int event,
						 void *callbackData, int eventData1, int eventData2)
{
	char Formula2[500]="";

	switch (event)
	{
		case EVENT_COMMIT:

			GetCtrlVal(panel,PANEL_ADD_FORMULA,		Formula2); //prendre la valeur de la formule
			SetCtrlVal(panel,PANEL_ADD_DURATION2,	Formula2);
			SetCtrlAttribute (panel, PANEL_ADD_PARAMETERS, ATTR_DFLT_VALUE, 0);
			DefaultCtrl (panel, PANEL_ADD_PARAMETERS);

			//Modif MaximePAGES 28/07/2020
			//	Value2 = parameterToValue (Formula2);
			Value2 = atof(parameterToValue_Str(Formula2));    //Modif MaximePAGES 05/08/2020

			SetCtrlVal(panel,PANEL_ADD_DURATION,Value2);
			SetCtrlVal(panel,PANEL_ADD_FORMULA,"");
			SetCtrlVal(panel,PANEL_ADD_VALUE,"");
			SetCtrlVal(panel,PANEL_ADD_UNIT,"");

			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_TIME,ATTR_DIMMED, 1); //from time
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_DURATION,ATTR_DIMMED, 1); //from duration
			
			dimmedNumericKeypadForScript(1); 
			
			
			break;
	}
	return 0;
}

// Fin des modifications LC

// Debut modif IHM tableau POPUP

// DEBUT ALGO
//******************************************************************************
//int CVICALLBACK SCRIPTS_DETAILS (int panel, int control, int event,
//								 void *callbackData, int eventData1, int eventData2)
//******************************************************************************
//	- Paramètres CVI
//
//  - Gestion des fichiers de données: menu
//
//  - Carolina modif : Sauvegarde excel (GST_SCRIPT_MAIN_SAVE_TEST_SCRIPT) et format .xlsx
//                     Nettoyage du code, confirmpop
//******************************************************************************
// FIN ALGO

//Modif MaximePAGES 10/08/2020 - New value and parameters insertion  ************  
int CVICALLBACK TimeTxt_Callback (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	if (event == EVENT_LEFT_CLICK)
	{
		dimmedNumericKeypadForScript(0);
		currentTxtBox = 1;
	}
	
	if (event == EVENT_KEYPRESS)
	{
		MessagePopup ("Message", "Please use the numeric keypad to enter your value.");
	}  
	
	return 0;
}

int CVICALLBACK DurationTxt_Callback (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	if (event == EVENT_LEFT_CLICK)
	{
		dimmedNumericKeypadForScript(0);
		currentTxtBox = 2; 
	}
	
	if (event == EVENT_KEYPRESS)
	{
		MessagePopup ("Message", "Please use the numeric keypad to enter your value.");
	}  
	
	return 0;
}


int CVICALLBACK PressTxt_Callback (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	if (event == EVENT_LEFT_CLICK)
	{

		dimmedNumericKeypadForScript(0);
		currentTxtBox = 3; 

	}
	if (event == EVENT_KEYPRESS)
	{
		MessagePopup ("Message", "Please use the numeric keypad to enter your value.");
	}  
	
	return 0;
}

int CVICALLBACK AccTxt_Callback (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	if (event == EVENT_LEFT_CLICK)
	{

		dimmedNumericKeypadForScript(0);
		currentTxtBox = 4; 

	}
	if (event == EVENT_KEYPRESS)
	{
		MessagePopup ("Message", "Please use the numeric keypad to enter your value.");
	}  
	
	return 0;
}

int CVICALLBACK InterframeTxt_Callback (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	if (event == EVENT_LEFT_CLICK)
	{

		dimmedNumericKeypadForScript(0);
		currentTxtBox = 5; 

	}
	if (event == EVENT_KEYPRESS)
	{
		MessagePopup ("Message", "Please use the numeric keypad to enter your value.");
	}  
	
	return 0;
}

int CVICALLBACK LFPowerTxt_Callback (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	if (event == EVENT_LEFT_CLICK)
	{

		dimmedNumericKeypadForScript(0);
		currentTxtBox = 6; 

	}
	if (event == EVENT_KEYPRESS)
	{
		MessagePopup ("Message", "Please use the numeric keypad to enter your value.");
	}  
	
	return 0;
}




//******************************************************************************  
  //MODIF MARVYN 18/06/2021 : load des expected results en même temps 

int CVICALLBACK SCRIPTS_DETAILS (int panel, int control, int event,
								 void *callbackData, int eventData1, int eventData2)
{
	Point cell;
	Rect  rect;
	BOOL bValid;

	int iError;
	int iX=0, iY=0, iNumberOfRows=0; //iInsertAfter=0;
	int iTop=0, iLeft=0;
	int iIdSelected=0;

	///////// Paramètre ADD
	char range[30];						// Point exel ex: A2 , en chaine de caractère

	char sCellValue[200]="";
	char sCellValue2[200]="";
	//char worksheetName[10]="TestCase";
	//char rangeColor[30];
	//char *sPre=NULL;
    //char *sScript=NULL;
    //char *sPost=NULL;
	char pathName[MAX_PATHNAME_LEN];
	//long *returnValue;
	//ERRORINFO *errorInfo;
	//int iTimeValue=0;
	char defaultDirectory[MAX_PATHNAME_LEN] = "D:\\";
	char currentDirectory[MAX_PATHNAME_LEN];
	/////////Parametre Load;
	int boucle=0;
	int EndBoucle=0;
	//char st[30];

	////////move
	int numeroLigne=0, errDir = 0;

	errDir = GetProjectDir(currentDirectory);
	if(errDir == -1 || errDir == -2)
	{
		FmtOut ("Project is untitled\n");
		strcpy(currentDirectory, defaultDirectory);
	}

	switch (event)
	{

		case EVENT_RIGHT_CLICK:



			if((GetGiStartCycleRun() == 0))
			{

				SetCtrlAttribute (panel, PANEL_MODE_SCRIPTS_DETAILS, ATTR_ENABLE_POPUP_MENU, 0);

				// Lecture de la position du panel (coordonnées en haut à gauche)
				GetPanelAttribute (panel, ATTR_LEFT, &iLeft);
				GetPanelAttribute (panel, ATTR_TOP, &iTop);

				iError = GstTablesGetRowColFromMouse(panel, control, &cell.y, &cell.x, &iX, &iY, &bValid);

				// Détermination du nombre de lignes du contrôle (data grid)
				iError = GetNumTableRows (panel, control, &iNumberOfRows);

				// Si le nombre de lignes est supérieur à zéro
				iError = GstTablesGstZoneSelect(panel, control, cell.y, cell.x,
												&rect.height, &rect.width, &rect.left,
												&rect.top, &bValid);

				//Menu selection
				iIdSelected = RunPopupMenu (GiPopupMenuScriptHandle, GST_SCRIPT_MAIN, panel, iY-iTop, iX-iLeft, 0, 0, 20, 20);
				switch(iIdSelected)
				{

					case GST_SCRIPT_MAIN_DELETE:
						if(iNumberOfRows == 0)
						{
							return 0;
						}
						else
						{
							if (rect.top != 0)
							{
							//DeleteTableRows (panelHandle, tableCtrl, index, numToDelete); //Programming with Table Controls
							//printf("1 = %d and 2 = %d\n",rect.top, rect.height);
							DeleteTableRows (panel, PANEL_MODE_SCRIPTS_DETAILS, rect.top, rect.height);
							*pointeurval= *pointeurval-1;
							}
							 
							return 0;
						}
					/*	if(*pointeurval > 0)
						{
							*pointeurval= *pointeurval-1;  // Retire 1 au compteur de ligne du tableau
						}   */
					
						break;

					case GST_SCRIPT_MAIN_DELETEALL:
						if(iNumberOfRows == 0)
						{
							return 0;
						}
						else
						{
							if(ConfirmPopup ("Delete All", "Do you want to delete all lines?"))
							{
								DeleteTableRows (panel, PANEL_MODE_SCRIPTS_DETAILS, 1, -1);
								*pointeurval=0;	// remet a 0 le compteur de ligne du tableau
							}
						}
					
						break;

					case GST_SCRIPT_MAIN_ADD:
						//Open Script definition windows
						if(!CheckPrecondition())
						{
							return 0;
						}
						insertSteps = 0;
						modifySteps = 0;
						add_menu();
					
						//Modif MaximePAGES 10/08/2020 ***************
						InstallCtrlCallback (GiPopupAdd2, PANEL_ADD_TIME2, TimeTxt_Callback, 0); 
						InstallCtrlCallback (GiPopupAdd2, PANEL_ADD_DURATION2, DurationTxt_Callback, 0);  
						InstallCtrlCallback (GiPopupAdd2, PANEL_ADD_PRESSURE, PressTxt_Callback, 0);  
						InstallCtrlCallback (GiPopupAdd2, PANEL_ADD_ACCELERATION, AccTxt_Callback, 0);  
						InstallCtrlCallback (GiPopupAdd2, PANEL_ADD_INTERFRAME, InterframeTxt_Callback, 0);
						InstallCtrlCallback (GiPopupAdd2, PANEL_ADD_LFPOWER, LFPowerTxt_Callback, 0);
						
						//********************************************
					
						break;
					//MARVYN 22/06/2021
					case GST_SCRIPT_MAIN_INSERT:
						
							if(!CheckPrecondition())
						{
							return 0;
						}
						//MARVYN 23/06/2021  && 23/07/2021
						color = 0;
						insertSteps = 1;
						modifySteps = 0; 
						numRow = rect.top ;
						if ( numRow != 0)
						{
						couleurSelection(numRow,PANEL_MODE_SCRIPTS_DETAILS, panel,VAL_LT_GRAY )  ;  
						//**************
						add_menu();
						
						
						//Modif MaximePAGES 10/08/2020 ***************
						InstallCtrlCallback (GiPopupAdd2, PANEL_ADD_TIME2, TimeTxt_Callback, 0); 
						InstallCtrlCallback (GiPopupAdd2, PANEL_ADD_DURATION2, DurationTxt_Callback, 0);  
						InstallCtrlCallback (GiPopupAdd2, PANEL_ADD_PRESSURE, PressTxt_Callback, 0);  
						InstallCtrlCallback (GiPopupAdd2, PANEL_ADD_ACCELERATION, AccTxt_Callback, 0);  
						InstallCtrlCallback (GiPopupAdd2, PANEL_ADD_INTERFRAME, InterframeTxt_Callback, 0);
						InstallCtrlCallback (GiPopupAdd2, PANEL_ADD_LFPOWER, LFPowerTxt_Callback, 0);	
						}
						
					break;

					case GST_SCRIPT_MAIN_MODIFY:
						//Open Script definition windows
						if(!CheckPrecondition())
						{
							return 0;
						}
						color = 0;
						numRow = rect.top ;
						tableHeight = rect.height ;
						modifySteps =1;
						insertSteps = 0;
						if ( numRow != 0)
						{
						couleurSelection(numRow,PANEL_MODE_SCRIPTS_DETAILS, panel,VAL_LT_GRAY )  ;
						add_menu();
						modifyStep(numRow);
						
	
						//Modif MaximePAGES 10/08/2020 ***************
						InstallCtrlCallback (GiPopupAdd2, PANEL_ADD_TIME2, TimeTxt_Callback, 0); 
						InstallCtrlCallback (GiPopupAdd2, PANEL_ADD_DURATION2, DurationTxt_Callback, 0);  
						InstallCtrlCallback (GiPopupAdd2, PANEL_ADD_PRESSURE, PressTxt_Callback, 0);  
						InstallCtrlCallback (GiPopupAdd2, PANEL_ADD_ACCELERATION, AccTxt_Callback, 0);  
						InstallCtrlCallback (GiPopupAdd2, PANEL_ADD_INTERFRAME, InterframeTxt_Callback, 0);
						InstallCtrlCallback (GiPopupAdd2, PANEL_ADD_LFPOWER, LFPowerTxt_Callback, 0);
						}
						
						break;
					
					
					 //MODIF MARVYN 18/06/2021
					case GST_SCRIPT_MAIN_LOAD_TEST_SCRIPT:   //this function doesn't work in run mode
					   	FileSelectPopupEx (currentDirectory, "*.xlsx", "*.xlsx", "Load script file", VAL_LOAD_BUTTON, 0, 1, pathName);
						//FileSelectPopup(currentDirectory,"*.xlsx","*.xlsx", "Load script file", VAL_LOAD_BUTTON, 0, 1, 1, 1, pathName);
						// Ouverture d'excel pour charger le fichier
						//ExcelRpt_ApplicationNew(VFALSE,&applicationHandleLoad);
						ExcelRpt_WorkbookOpen (applicationHandleProject, pathName, &workbookHandleLoad);
						ExcelRpt_GetWorksheetFromIndex(workbookHandleLoad, 2, &worksheetHandleLoad);
						ExcelRpt_GetCellValue (worksheetHandleLoad, "I1", CAVT_INT,&EndBoucle);
						
						//***********************MARVYN 29/06/2021******************************
						//printf("pathname2 = %s\n",pathName);
						//printf("pathname = %d\n",pathName[strlen(pathName)-1]);
						if (strcmp(pathName,"")!=0)
						{
						if ( pathName[strlen(pathName)-1] != 25 && pathName[strlen(pathName)-1] != 8 ) 
						{
						DeleteTableRows (panel, PANEL_MODE_SCRIPTS_DETAILS, 1, -1);
						*pointeurval=0;	// remet a 0 le compteur de ligne du tableau
					
						DeleteTableRows (GiPanel, PANEL_MODE_EXP_RESULTS, 1, -1);
						*PointeurNbRowsExpR=0;	// remet a 0 le compteur de ligne du tableau
										  
						
						
						*PointeurNbRowsExpR= EndBoucle-1;

						// charge chaque lignes du tableau
						for(boucle=1; boucle <= EndBoucle-1; boucle++)
						{

							InsertTableRows (GiPanel, PANEL_MODE_EXP_RESULTS, -1, 1, VAL_USE_MASTER_CELL_TYPE);

							cell.y =boucle;
							//x number of columns
							cell.x =1;  //Command check
							sprintf(range,"A%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandleLoad, range, CAVT_CSTRING,sCellValue);
							SetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);

							cell.x =2;  //Function code value
							sprintf(range,"B%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandleLoad,range , CAVT_CSTRING,sCellValue);
							SetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);

							cell.x =3;  //labels
							sprintf(range,"C%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandleLoad,range , CAVT_CSTRING,sCellValue);
							SetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);

							cell.x =4;  //interface
							sprintf(range,"D%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandleLoad,range , CAVT_CSTRING,sCellValue);
							SetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);

							cell.x =5;  //parameters
							sprintf(range,"E%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandleLoad,range , CAVT_CSTRING,sCellValue);
							SetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);

							cell.x =6;  //Value
							sprintf(range,"F%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandleLoad,range , CAVT_CSTRING,sCellValue);
							SetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);

							cell.x =7;  //tolerence
							sprintf(range,"G%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandleLoad,range , CAVT_CSTRING,sCellValue);
							SetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);

							cell.x =8;  //tolerence %
							sprintf(range,"H%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandleLoad,range , CAVT_CSTRING,sCellValue);
							SetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);

							//	couleurLigne(boucle, panel);
						}
						
					//	ExcelRpt_GetWorksheetFromIndex(workbookHandleLoad, 1, &worksheetHandleLoad);
					//	ExcelRpt_GetCellValue (worksheetHandleLoad, "I1", CAVT_INT,&EndBoucle);
						ExcelRpt_WorkbookClose (workbookHandleLoad, 0);
						ExcelRpt_WorkbookOpen (applicationHandleProject, pathName, &workbookHandle7);
						ExcelRpt_GetWorksheetFromIndex(workbookHandle7, 1, &worksheetHandle7);
						ExcelRpt_GetCellValue (worksheetHandle7, "I1", CAVT_INT,&EndBoucle);

						*pointeurval= EndBoucle-1;

						// charge chaque lignes du tableau
						for(boucle=1; boucle <= EndBoucle-1; boucle++)
						{

							InsertTableRows (panel, PANEL_MODE_SCRIPTS_DETAILS, -1, 1, VAL_USE_MASTER_CELL_TYPE);

							cell.y =boucle;
							//x number of columns
							cell.x =1;  //Time
							sprintf(range,"A%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandle7, range, CAVT_CSTRING,sCellValue);
							SetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);

							cell.x =2;  //Duration
							sprintf(range,"B%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandle7,range , CAVT_CSTRING,sCellValue);
							SetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);

							
						
							cell.x =3;  //Function
							sprintf(range,"C%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandle7,range , CAVT_CSTRING,sCellValue);
							SetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
							
						
							cell.x =4;  //Value
							sprintf(range,"D%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandle7,range , CAVT_CSTRING,sCellValue);
							SetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);

							cell.x =5;  //Nb of Frames
							sprintf(range,"E%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandle7,range , CAVT_CSTRING,sCellValue);
							SetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);

							cell.x =6;  //WU ID
							sprintf(range,"F%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandle7,range , CAVT_CSTRING,sCellValue);
							SetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);

							cell.x =7;  //Interframe
							sprintf(range,"G%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandle7,range , CAVT_CSTRING,sCellValue);
							SetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);

							cell.x =8;  //Comments
							sprintf(range,"H%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandle7,range , CAVT_CSTRING,sCellValue);
							SetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);

							couleurLigne(boucle, panel);
						}

						/*cell.x =1; //END line
						sprintf(range,"A%d",boucle+1);
						ExcelRpt_GetCellValue (worksheetHandle3, range, CAVT_INT,&iCellValue);
						SetCtrlVal(panel,PANEL_MODE_ENDTIME,iCellValue); */
						}
						}
						ExcelRpt_WorkbookClose (workbookHandle7, 0);
						//ExcelRpt_ApplicationQuit (applicationHandle7);
						//CA_DiscardObjHandle(applicationHandle7);
						
					
						break;

					case GST_SCRIPT_MAIN_MOVE_UP:

						numeroLigne = rect.top;

						if(numeroLigne<2)
						{
						}
						else
						{

							for(boucle=1; boucle < 9 ; boucle++)  //9 ??
							{
								cell.x=boucle; // 1 
								cell.y =numeroLigne; // 3
								GetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue); //collect info for the line we want to move
								cell.y =numeroLigne-1;  // move 1 row up 
								GetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue2);
								SetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
								cell.y =numeroLigne;	//move 1 row down after have exchanged the values
								SetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue2);

							}

							couleurLigne(numeroLigne, panel);
							couleurLigne(numeroLigne-1, panel);

						}
						
						break;

					case GST_SCRIPT_MAIN_MOVE_DOWN:

						numeroLigne = rect.top;

						if(numeroLigne==*pointeurval)
						{
						}
						else
						{
							for(boucle=1; boucle < 9 ; boucle++)
							{
								cell.x=boucle;
								cell.y =numeroLigne;
								GetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
								cell.y =numeroLigne+1;
								GetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue2);
								SetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
								cell.y =numeroLigne;
								SetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue2);
							}

							couleurLigne(numeroLigne, panel);
							couleurLigne(numeroLigne+1, panel);

						}
						
						break;
				}

			}
			break;

	}
	
	return 0;
}




//***********************************MARVYN 24/06/2021*********************************************

void modifyStep(int rowNb)
{
	Point cell;
	int panelHandle = 0;
	char sCellValue[200]="";
	char type[15]="";

	//***********************************************MARVYN**********************
	char stringNbframe[50] = ""; 


			for (int i=1; i<256; i++)
			{
				sprintf(stringNbframe, "%d",i);  // on convertir le int en string
				InsertListItem (GiPopupAdd2,PANEL_ADD_LFDNBFRAME,i,stringNbframe,stringNbframe);
			}
	
	
	  //**********************************************
	
	
	
	panelHandle = LoadPanel(0, "IhmModes.uir",PANEL_ADD);
	SetCtrlAttribute(panelHandle-2,PANEL_ADD_FUNCTION_SELECT,ATTR_DIMMED,1);
	cell.x=3;
	cell.y=rowNb;
	
	GetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
	
	cell.x=8;
	GetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS,cell, type);
	
	if (strcmp(type,"Script")==0)
	{
		SetCtrlVal(panelHandle-2,PANEL_ADD_BOX_SCRIPT,1);
		SetCtrlVal(panelHandle-2,PANEL_ADD_BOX_PRE,0);
		SetCtrlVal(panelHandle-2,PANEL_ADD_BOX_POST,0);
	}
	else if (strcmp(type,"Pre")==0)
	{
		SetCtrlVal(panelHandle-2,PANEL_ADD_BOX_SCRIPT,0);
		SetCtrlVal(panelHandle-2,PANEL_ADD_BOX_PRE,1);
		SetCtrlVal(panelHandle-2,PANEL_ADD_BOX_POST,0);
	}
	else if (strcmp(type,"Post")==0)
	{
		SetCtrlVal(panelHandle-2,PANEL_ADD_BOX_SCRIPT,0);
		SetCtrlVal(panelHandle-2,PANEL_ADD_BOX_PRE,0);
		SetCtrlVal(panelHandle-2,PANEL_ADD_BOX_POST,1);
	}
	
	
	if (strcmp(sCellValue, "SetA") ==0)
	{
	 	SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_TIME2,ATTR_DIMMED,0);
		SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_DURATION2,ATTR_DIMMED,0);
		SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT_PARAMETER,ATTR_DIMMED,0);
		SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFPOWER,ATTR_DIMMED,1);
		SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_PRESSURE,ATTR_DIMMED,1);
		SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_ACCELERATION,ATTR_DIMMED,0); 
		SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT_PARAMETER2,ATTR_DIMMED,0);	  
		SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFDNAME,ATTR_DIMMED,1);
		SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFDNBFRAME,ATTR_DIMMED,1); //AJOUT Maxime PAGES - 10/06/2020
		SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INTERFRAME,ATTR_DIMMED,1);
		SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID,ATTR_DIMMED,1);
		SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LABEL_STRING,ATTR_DIMMED, 1);
		SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID_VALUE,ATTR_DIMMED,1);
		
	 	cell.x = 1 ;
		GetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
	    SetCtrlVal(panelHandle-2,PANEL_ADD_TIME2,sCellValue); 
		cell.x = 2 ; 
		GetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
	    SetCtrlVal(panelHandle-2,PANEL_ADD_DURATION2,sCellValue); 
		cell.x = 4 ;
		GetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
		sCellValue[strlen(sCellValue)-2]=NULL ;
	    SetCtrlVal(panelHandle-2,PANEL_ADD_ACCELERATION,sCellValue); 
		command = 1;
				 
	}
	else if (strcmp(sCellValue, "SendLFD") ==0)
	{
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT_PARAMETER,		ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFPOWER,					ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_PRESSURE,				ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_ACCELERATION,			ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT_PARAMETER2,		ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFDNAME,					ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFDNBFRAME,				ATTR_DIMMED,0); 
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INTERFRAME,				ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID,					ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LABEL_STRING,			ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID_VALUE,				ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_TIME2,					ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_DURATION2,				ATTR_DIMMED,1);
		
			cell.x = 1 ;
			GetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
		    SetCtrlVal(panelHandle-2,PANEL_ADD_TIME2,sCellValue); 
			cell.x = 2 ; 
			GetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
		    SetCtrlVal(panelHandle-2,PANEL_ADD_DURATION2,sCellValue); 
			cell.x = 4 ;
			GetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
		    SetCtrlVal(panelHandle-2,PANEL_ADD_LFDNAME,sCellValue); 
			cell.x = 5 ;
			GetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
		    SetCtrlVal(panelHandle-2,PANEL_ADD_LFDNBFRAME,sCellValue); 
			cell.x = 7 ;
			GetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
		    SetCtrlVal(panelHandle-2,PANEL_ADD_INTERFRAME,sCellValue); 
			cell.x = 6 ;
			GetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
		    SetCtrlVal(panelHandle-2,PANEL_ADD_WUID,10);
		    SetCtrlVal(panelHandle-2,PANEL_ADD_LFPOWER,"20"); 
			command = 3;
				 
			   
	}
	else if (strcmp(sCellValue, "SetP") ==0)
	{
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_TIME2,ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_DURATION2,ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT_PARAMETER,ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFPOWER,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_PRESSURE,ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_ACCELERATION,ATTR_DIMMED,1); 
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT_PARAMETER2,ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFDNAME,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFDNBFRAME,ATTR_DIMMED,1); 
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INTERFRAME,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LABEL_STRING,ATTR_DIMMED, 1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID_VALUE,ATTR_DIMMED,1);
			
			
			cell.x = 1 ;
			GetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
		    SetCtrlVal(panelHandle-2,PANEL_ADD_TIME2,sCellValue); 
			cell.x = 2 ; 
			GetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
		    SetCtrlVal(panelHandle-2,PANEL_ADD_DURATION2,sCellValue); 
			cell.x = 4 ;
			GetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
			sCellValue[strlen(sCellValue)-2]=NULL ;  
		    SetCtrlVal(panelHandle-2,PANEL_ADD_PRESSURE,sCellValue); 
			command = 2;
			     
	}
	else if (strcmp(sCellValue, "StopLFD") ==0)
	{
		
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT_PARAMETER,ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFPOWER,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_PRESSURE,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_ACCELERATION,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT_PARAMETER2,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFDNAME,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFDNBFRAME,ATTR_DIMMED,1); 
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INTERFRAME,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_TIME2,ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_DURATION2,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LABEL_STRING,ATTR_DIMMED, 1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID_VALUE,ATTR_DIMMED,1);
			command = 6; 
			
			cell.x = 1 ;
			GetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
		    SetCtrlVal(panelHandle-2,PANEL_ADD_TIME2,sCellValue);
			   
	}
	else if (strcmp(sCellValue, "StopLFCw") ==0)
	{
		
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT_PARAMETER,		ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFPOWER,					ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_PRESSURE,				ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_ACCELERATION,			ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT_PARAMETER2,		ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFDNAME,					ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFDNBFRAME,				ATTR_DIMMED,1); 
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INTERFRAME,				ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID,					ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_TIME2,					ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_DURATION2,				ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LABEL_STRING,			ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID_VALUE,				ATTR_DIMMED,1);
		    command = 7;
			
			cell.x = 1 ;
			GetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
		    SetCtrlVal(panelHandle-2,PANEL_ADD_TIME2,sCellValue);
	}
	else if (strcmp(sCellValue, "SendLFCw") ==0)
	{
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT_PARAMETER,		ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFPOWER,					ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_PRESSURE,				ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_ACCELERATION,			ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT_PARAMETER2,		ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFDNAME,					ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFDNBFRAME,				ATTR_DIMMED,0); 
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INTERFRAME,				ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID,					ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_TIME2,					ATTR_DIMMED,0); 
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_DURATION2,				ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LABEL_STRING,			ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID_VALUE,				ATTR_DIMMED,0);
		
			cell.x = 1 ;
			GetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
		    SetCtrlVal(panelHandle-2,PANEL_ADD_TIME2,sCellValue); 
			cell.x = 2 ; 
			GetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
		    SetCtrlVal(panelHandle-2,PANEL_ADD_DURATION2,sCellValue); 
			cell.x = 4 ;
			GetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
		    SetCtrlVal(panelHandle-2,PANEL_ADD_LFDNAME,sCellValue); 
			cell.x = 5 ;
			GetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
		    SetCtrlVal(panelHandle-2,PANEL_ADD_LFDNBFRAME,sCellValue); 
			cell.x = 7 ;
			GetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
		    SetCtrlVal(panelHandle-2,PANEL_ADD_INTERFRAME,sCellValue); 
			cell.x = 6 ;
			GetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
		    SetCtrlVal(panelHandle-2,PANEL_ADD_WUID,sCellValue); 
			GetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
		    SetCtrlVal(panelHandle-2,PANEL_ADD_LFPOWER,"20"); 
			command = 4;
			
	}
	else if (strcmp(sCellValue, "SendLFPower") ==0)
	{
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT_PARAMETER,ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFPOWER,ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_PRESSURE,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_ACCELERATION,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT_PARAMETER2,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFDNAME,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFDNBFRAME,ATTR_DIMMED,1); 
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INTERFRAME,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_TIME2,ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_DURATION2,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LABEL_STRING,ATTR_DIMMED, 1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID_VALUE,ATTR_DIMMED,1);
			
			cell.x = 1 ;
			GetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
		    SetCtrlVal(panelHandle-2,PANEL_ADD_TIME2,sCellValue); 
			GetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
		    SetCtrlVal(panelHandle-2,PANEL_ADD_LFPOWER,"20"); 
			command = 5;
	}
	else 
	{		
		    SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT_PARAMETER,ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFPOWER,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_PRESSURE,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_ACCELERATION,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT_PARAMETER2,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFDNAME,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LFDNBFRAME,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INTERFRAME,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_TIME2,ATTR_DIMMED,0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_DURATION2,ATTR_DIMMED,1);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_LABEL_STRING,ATTR_DIMMED, 0);
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID_VALUE,ATTR_DIMMED,1);
			
			cell.x = 1 ;
			GetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
		    SetCtrlVal(panelHandle-2,PANEL_ADD_TIME2,sCellValue); 
			cell.x = 3 ;
			GetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
			//printf("value label = %s\n",sCellValue);
		    SetCtrlVal(panelHandle-2,PANEL_ADD_LABEL_STRING,sCellValue);
			strcpy(labelModif,sCellValue);
			command = 8;
	}   
			
}

//MARVYN 28/06/2021
//Move of one letter to the left
void MoveString(char *label)
{
	//char newLabel[strlen(label)-1] ;
 	for (int i=0;i<strlen(label)-1;i++)
 	{
		label[i]=label[i+1];
	}
	label[i]=NULL;
}


/*char* MoveString(char *label)
{
	char newLabel[strlen(label)-1] ;
 	for (int i=0;i<strlen(label)-2;i++)
 	{
		newLabel[i]=label[i+1];
	}
	label[i+1]=NULL;
}*/


//***********************************MARVYN 24/06/2021*********************************************
//MARVYN 28/06/2021  
void modifyStepExpResult(int rowNb)
{
	Point cell;
	int panelHandle = 0;
	int panelHandle2 = 0;
	char sCellValue[200]="";
	
	char *token;

	
	
	
	
	panelHandle = LoadPanel(0, "IhmModes.uir",EXPRESULTS);
	panelHandle2 = LoadPanel(0, "IhmModes.uir",PANEL_ADD);


	SetCtrlAttribute(panelHandle - 3,EXPRESULTS_EXPLIST,ATTR_DIMMED,1);

	cell.x=1;
	cell.y=rowNb;
	GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
	
	if (strcmp(sCellValue, "CheckTimingInterFrames") ==0)
	{
	 	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_LABEL1,			ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_LABEL2,			ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_VALUE,			ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TXTUNITLABEL,	ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDVAL,			ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DELVALUE,		ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG,			ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE1,		ATTR_DIMMED,1);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDTOL,			ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE2,		ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDTOL_2,		ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDEXP,			ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA,	ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_VALUEFC,			ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,		ATTR_DIMMED,1);
		SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_TXTUNITBODY, "[s]"); 
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BTNTOLVAL,		ATTR_DIMMED,0); 
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BTNTOLPRCT,		ATTR_DIMMED,0); 
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_FIXEDVALUE,ATTR_DIMMED,0); 
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_SEQVALUE,	 ATTR_DIMMED,0); 
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_RANGEVALUE,ATTR_DIMMED,1); 
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DECORATION_7,	ATTR_VISIBLE, 0); 
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTINCH,		ATTR_VISIBLE, 0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_WHEELDIM,		ATTR_VISIBLE, 0); 
	 
		
	 	cell.x = 2 ;
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
	    SetCtrlVal(panelHandle - 3,EXPRESULTS_VALUEFC,sCellValue); 
		
		cell.x = 3 ; 
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
		token = strtok(sCellValue, "|");
		token[strlen(token)-1]=NULL;
		SetCtrlVal(panelHandle - 3,EXPRESULTS_LABEL1,token);
		token = strtok(NULL, "|");
		token[strlen(token)-1]=NULL;
		MoveString(token);
		SetCtrlVal(panelHandle - 3,EXPRESULTS_LABEL2,token);
		
		cell.x = 4 ;
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
	    SetCtrlVal(panelHandle - 3,EXPRESULTS_PARAMETERSDATA,0); 

		cell.x = 5 ;
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
	    SetCtrlVal(panelHandle - 3,EXPRESULTS_FIELDCHECK,0); 

		cell.x = 6 ;
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
		sCellValue[strlen(sCellValue)-1]=NULL;
	    SetCtrlVal(panelHandle - 3,EXPRESULTS_VALUE,sCellValue);

		cell.x = 7 ;
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
		
		//printf("sCellValue=%s\n",sCellValue);
		if (strcmp(sCellValue,"0;")==0)
		{
		cell.x = 8 ;
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE1,ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE2,ATTR_DIMMED,1);
		SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_BTNTOLVAL,0); 
		SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_BTNTOLPRCT,1); 
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
		sCellValue[strlen(sCellValue)-1]=NULL;
	    SetCtrlVal(panelHandle - 3,EXPRESULTS_TOLERENCE1,sCellValue);
		}
		else
		{
		sCellValue[strlen(sCellValue)-1]=NULL;
		SetCtrlVal(panelHandle - 3,EXPRESULTS_TOLERENCE2,sCellValue);
		}
		
		commandExpResult = 1;
	}
	else if (strcmp(sCellValue, "CheckTimingInterBursts") ==0)
	{	    
	
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_LABEL1,			ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_LABEL2,			ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_VALUE,			ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TXTUNITLABEL,	ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDVAL,			ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DELVALUE,			ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG,			ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE1,		ATTR_DIMMED,1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDTOL,			ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE2,		ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDTOL_2,			ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDEXP,			ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA,	ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_VALUEFC,			ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,		ATTR_DIMMED,1);
			SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_TXTUNITBODY, "[s]"); 
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BTNTOLVAL,		ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BTNTOLPRCT,		ATTR_DIMMED,0); 
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_FIXEDVALUE,ATTR_DIMMED,0); 
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_SEQVALUE,	 ATTR_DIMMED,1); 
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_RANGEVALUE,ATTR_DIMMED,1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DECORATION_7,	ATTR_VISIBLE, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTINCH,		ATTR_VISIBLE, 0); 
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_WHEELDIM,		ATTR_VISIBLE, 0);

		cell.x = 2 ;
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
	    SetCtrlVal(panelHandle - 3,EXPRESULTS_VALUEFC,sCellValue); 
		
		cell.x = 3 ; 
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
		token = strtok(sCellValue, "|");
		token[strlen(token)-1]=NULL;
		SetCtrlVal(panelHandle - 3,EXPRESULTS_LABEL1,token);
		token = strtok(NULL, "|");
		token[strlen(token)-1]=NULL;
		MoveString(token);
		SetCtrlVal(panelHandle - 3,EXPRESULTS_LABEL2,token);
		
		cell.x = 4 ;
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
	    SetCtrlVal(panelHandle - 3,EXPRESULTS_PARAMETERSDATA,0); 

		cell.x = 5 ;
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
	    SetCtrlVal(panelHandle - 3,EXPRESULTS_FIELDCHECK,0); 

		cell.x = 6 ;
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
		sCellValue[strlen(sCellValue)-1]=NULL;
	    SetCtrlVal(panelHandle - 3,EXPRESULTS_VALUE,sCellValue);

		cell.x = 7 ;
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
		if (strcmp(sCellValue,"0;")==0)
		{
		cell.x = 8 ;
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE1,ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE2,ATTR_DIMMED,1);
		SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_BTNTOLVAL,0); 
		SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_BTNTOLPRCT,1); 
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
		sCellValue[strlen(sCellValue)-1]=NULL;
	    SetCtrlVal(panelHandle - 3,EXPRESULTS_TOLERENCE1,sCellValue);
		}
		else
		{
		sCellValue[strlen(sCellValue)-1]=NULL;
		SetCtrlVal(panelHandle - 3,EXPRESULTS_TOLERENCE2,sCellValue);
		}
		
		
		
		commandExpResult = 2;
	
	}
	else if (strcmp(sCellValue, "CheckFieldValue") ==0)
	{
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_LABEL1,			ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_LABEL2,			ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_VALUE,			ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TXTUNITLABEL,		ATTR_DIMMED,1);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDVAL,			ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DELVALUE,			ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG,			ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE1,		ATTR_DIMMED,1);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDTOL,			ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDTOL_2,			ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE2,		ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDEXP,			ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA,	ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_VALUEFC,			ATTR_DIMMED,1);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,		ATTR_DIMMED,1);
		SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_TXTUNITBODY, "[s,g,kPa...]"); 
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BTNTOLVAL,		ATTR_DIMMED,0); 
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BTNTOLPRCT,		ATTR_DIMMED,0); 
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_FIXEDVALUE,ATTR_DIMMED,0); 
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_SEQVALUE,	 ATTR_DIMMED,0); 
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_RANGEVALUE,ATTR_DIMMED,0); 
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DECORATION_7,	ATTR_VISIBLE, 0); 
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTINCH,		ATTR_VISIBLE, 0); 
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_WHEELDIM,		ATTR_VISIBLE, 0); 

		cell.x = 3 ; 
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
		token = strtok(sCellValue, "|");
		token[strlen(token)-1]=NULL;
		SetCtrlVal(panelHandle - 3,EXPRESULTS_LABEL1,token);
		token = strtok(NULL, "|");
		token[strlen(token)-1]=NULL;
		MoveString(token);
		SetCtrlVal(panelHandle - 3,EXPRESULTS_LABEL2,token);
		
		cell.x = 4 ;
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
	    SetCtrlVal(panelHandle - 3,EXPRESULTS_PARAMETERSDATA,0); 

		cell.x = 5 ;
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
	    SetCtrlVal(panelHandle - 3,EXPRESULTS_FIELDCHECK,0); 

		cell.x = 6 ;
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
		sCellValue[strlen(sCellValue)-1]=NULL;
	    SetCtrlVal(panelHandle - 3,EXPRESULTS_VALUE,sCellValue);

		cell.x = 7 ;
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
		if (strcmp(sCellValue,"0;")==0)
		{
		cell.x = 8 ;
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE1,ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE2,ATTR_DIMMED,1);
		SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_BTNTOLVAL,0); 
		SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_BTNTOLPRCT,1); 
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
		sCellValue[strlen(sCellValue)-1]=NULL;
	    SetCtrlVal(panelHandle - 3,EXPRESULTS_TOLERENCE1,sCellValue);
		}
		else
		{
		sCellValue[strlen(sCellValue)-1]=NULL;
		SetCtrlVal(panelHandle - 3,EXPRESULTS_TOLERENCE2,sCellValue);
		}
		
			commandExpResult = 3;     
	}
	else if (strcmp(sCellValue, "CheckSTDEV") ==0)
	{
				SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_VALUEFC,			ATTR_DIMMED,0);
				SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_LABEL1,			ATTR_DIMMED,0);
				SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_LABEL2,			ATTR_DIMMED,0);
				SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_VALUE,			ATTR_DIMMED,0);
				SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TXTUNITLABEL,		ATTR_DIMMED,0);
				SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDVAL,			ATTR_DIMMED,0);
				SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DELVALUE,			ATTR_DIMMED,0);
				SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG,			ATTR_DIMMED,0);
				SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE1,		ATTR_DIMMED,1);
				SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE2,		ATTR_DIMMED,1);
				SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDTOL,			ATTR_DIMMED,1);
				SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDTOL_2,			ATTR_DIMMED,1);
				SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDEXP,			ATTR_DIMMED,0);
				SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA,	ATTR_DIMMED,0);
				SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,		ATTR_DIMMED,1);
				SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BTNTOLVAL,		ATTR_DIMMED,1); 
				SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BTNTOLPRCT,		ATTR_DIMMED,1); 
				SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_TXTUNITBODY, "[degree]"); 
				SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_FIXEDVALUE,ATTR_DIMMED,1);
				SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_SEQVALUE,	 ATTR_DIMMED,1);
				SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_RANGEVALUE,ATTR_DIMMED,0); 
				SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DECORATION_7,	ATTR_VISIBLE, 0); 
				SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTINCH,		ATTR_VISIBLE, 0); 
				SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_WHEELDIM,		ATTR_VISIBLE, 0); 
				SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA,	ATTR_DIMMED,1);
				
				cell.x = 2 ;
				GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
	   			SetCtrlVal(panelHandle - 3,EXPRESULTS_VALUEFC,sCellValue); 
		
				cell.x = 3 ; 
				GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
				token = strtok(sCellValue, "|");
				token[strlen(token)-1]=NULL;
				SetCtrlVal(panelHandle - 3,EXPRESULTS_LABEL1,token);
				token = strtok(NULL, "|");
				token[strlen(token)-1]=NULL;
				MoveString(token);
				SetCtrlVal(panelHandle - 3,EXPRESULTS_LABEL2,token);
		
				cell.x = 4 ;
				GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
			    SetCtrlVal(panelHandle - 3,EXPRESULTS_PARAMETERSDATA,0); 

				cell.x = 5 ;
				GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
			    SetCtrlVal(panelHandle - 3,EXPRESULTS_FIELDCHECK,0); 

				cell.x = 6 ;
				GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
				sCellValue[strlen(sCellValue)-1]=NULL;
			    SetCtrlVal(panelHandle - 3,EXPRESULTS_VALUE,sCellValue);
				
		    commandExpResult = 4;     
			   
	}
	else if (strcmp(sCellValue, "CheckNbBursts") ==0)
	{
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_LABEL1,			ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_LABEL2,			ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_VALUE,			ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TXTUNITLABEL,		ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDVAL,			ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DELVALUE,			ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG,			ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE1,		ATTR_DIMMED,1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE2,		ATTR_DIMMED,1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDTOL,			ATTR_DIMMED,1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDTOL_2,			ATTR_DIMMED,1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDEXP,			ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA,	ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_VALUEFC,			ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,		ATTR_DIMMED,1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BTNTOLVAL,		ATTR_DIMMED,1); 
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BTNTOLPRCT,		ATTR_DIMMED,1); 
			SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_TXTUNITBODY, "bursts"); 
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_FIXEDVALUE,ATTR_DIMMED,0); 
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_SEQVALUE,	 ATTR_DIMMED,1); 
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_RANGEVALUE,ATTR_DIMMED,1); 
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DECORATION_7,	ATTR_VISIBLE, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTINCH,		ATTR_VISIBLE, 0); 
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_WHEELDIM,		ATTR_VISIBLE, 0); 
		
			cell.x = 2 ;
			GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
	   		SetCtrlVal(panelHandle - 3,EXPRESULTS_VALUEFC,sCellValue); 
		
			cell.x = 3 ; 
			GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
			token = strtok(sCellValue, "|");
			token[strlen(token)-1]=NULL; 
			SetCtrlVal(panelHandle - 3,EXPRESULTS_LABEL1,token);
			token = strtok(NULL, "|");
			token[strlen(token)-1]=NULL;
			MoveString(token);
			SetCtrlVal(panelHandle - 3,EXPRESULTS_LABEL2,token);
		
			cell.x = 4 ;
			GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
			SetCtrlVal(panelHandle - 3,EXPRESULTS_PARAMETERSDATA,0); 

			cell.x = 5 ;
			GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
			SetCtrlVal(panelHandle - 3,EXPRESULTS_FIELDCHECK,0); 

			cell.x = 6 ;
			GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
			sCellValue[strlen(sCellValue)-1]=NULL;
			SetCtrlVal(panelHandle - 3,EXPRESULTS_VALUE,sCellValue);
			
			commandExpResult = 5;                    
			
			
	}
	else if (strcmp(sCellValue, "CheckNbFramesInBurst") ==0)
	{
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_LABEL1,			ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_LABEL2,			ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_VALUE,			ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TXTUNITLABEL,		ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDVAL,			ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DELVALUE,			ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG,			ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE1,		ATTR_DIMMED,1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE2,		ATTR_DIMMED,1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDTOL,			ATTR_DIMMED,1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDTOL_2,			ATTR_DIMMED,1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDEXP,			ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA,	ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_VALUEFC,			ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,		ATTR_DIMMED,1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BTNTOLVAL,		ATTR_DIMMED,1); 
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BTNTOLPRCT,		ATTR_DIMMED,1); 
			SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_TXTUNITBODY, "frames"); 
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_FIXEDVALUE,ATTR_DIMMED,0); 
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_SEQVALUE,	 ATTR_DIMMED,1); 
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_RANGEVALUE,ATTR_DIMMED,1); 
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DECORATION_7,	ATTR_VISIBLE, 0); 
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTINCH,		ATTR_VISIBLE, 0); 
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_WHEELDIM,		ATTR_VISIBLE, 0); 

			cell.x = 2 ;
			SetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
	   		SetCtrlVal(panelHandle - 3,EXPRESULTS_VALUEFC,sCellValue); 
		
			cell.x = 3 ; 
			GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
			token = strtok(sCellValue, "|");
			token[strlen(token)-1]=NULL; 
			SetCtrlVal(panelHandle - 3,EXPRESULTS_LABEL1,token);
			token = strtok(NULL, "|");
			token[strlen(token)-1]=NULL;
			MoveString(token);
			SetCtrlVal(panelHandle - 3,EXPRESULTS_LABEL2,token);
		
			cell.x = 4 ;
			GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
			SetCtrlVal(panelHandle - 3,EXPRESULTS_PARAMETERSDATA,0); 

			cell.x = 5 ;
			GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
			SetCtrlVal(panelHandle - 3,EXPRESULTS_FIELDCHECK,0); 

			cell.x = 6 ;
			GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
			sCellValue[strlen(sCellValue)-1]=NULL;
			SetCtrlVal(panelHandle - 3,EXPRESULTS_VALUE,sCellValue);
			
			commandExpResult = 6; 
			
	}
	else if (strcmp(sCellValue, "CheckCompareP") ==0)
	{
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_LABEL1,			ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_LABEL2,			ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_VALUE,			ATTR_DIMMED,1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TXTUNITLABEL,		ATTR_DIMMED,1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDVAL,			ATTR_DIMMED,1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DELVALUE,			ATTR_DIMMED,1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG,			ATTR_DIMMED,1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE1,		ATTR_DIMMED,1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDTOL,			ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE2,		ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDTOL,			ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDTOL_2,			ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_VALUEFC,			ATTR_DIMMED,1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDEXP,			ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA,	ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,		ATTR_DIMMED,1);
			SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_TXTUNITBODY, ""); 
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BTNTOLVAL,		ATTR_DIMMED,0); 
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BTNTOLPRCT,		ATTR_DIMMED,0); 
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_FIXEDVALUE,ATTR_DIMMED,1); 
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_SEQVALUE,	 ATTR_DIMMED,1); 
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_RANGEVALUE,ATTR_DIMMED,1); 
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DECORATION_7,	ATTR_VISIBLE, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTINCH,		ATTR_VISIBLE, 0); 
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_WHEELDIM,		ATTR_VISIBLE, 0); 
	
			
		 
		
			cell.x = 3 ; 
			GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
			token = strtok(sCellValue, "|");
			token[strlen(token)-1]=NULL; 
			SetCtrlVal(panelHandle - 3,EXPRESULTS_LABEL1,token);
			token = strtok(NULL, "|");
			token[strlen(token)-1]=NULL;
			MoveString(token);
			SetCtrlVal(panelHandle - 3,EXPRESULTS_LABEL2,token);
		
			cell.x = 4 ;
			GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
			SetCtrlVal(panelHandle - 3,EXPRESULTS_PARAMETERSDATA,0);
			
			cell.x = 5 ;
			GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
			SetCtrlVal(panelHandle - 3,EXPRESULTS_FIELDCHECK,0); 

			 cell.x = 7 ;
			GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
			if (strcmp(sCellValue,"0;")==0)
			{
			cell.x = 8 ;
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE1,ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE2,ATTR_DIMMED,1);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_BTNTOLVAL,0); 
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_BTNTOLPRCT,1); 
			GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
			sCellValue[strlen(sCellValue)-1]=NULL;
		    SetCtrlVal(panelHandle - 3,EXPRESULTS_TOLERENCE1,sCellValue);
			}
			else
			{
			sCellValue[strlen(sCellValue)-1]=NULL;
			SetCtrlVal(panelHandle - 3,EXPRESULTS_TOLERENCE2,sCellValue);
			}
			
			commandExpResult = 7;   
	}
	else if (strcmp(sCellValue, "CheckCompareAcc") ==0) 
	{		
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_LABEL1,			ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_LABEL2,			ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_VALUE,			ATTR_DIMMED,1);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TXTUNITLABEL,		ATTR_DIMMED,1);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDVAL,			ATTR_DIMMED,1);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DELVALUE,			ATTR_DIMMED,1);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG,			ATTR_DIMMED,1);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE1,		ATTR_DIMMED,1);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDTOL,			ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE2,		ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDTOL,			ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDTOL_2,			ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDEXP,			ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_VALUEFC,			ATTR_DIMMED,1);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA,	ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,		ATTR_DIMMED,1);
		SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_TXTUNITBODY, ""); 
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BTNTOLVAL,		ATTR_DIMMED,0); 
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BTNTOLPRCT,		ATTR_DIMMED,0); 
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_FIXEDVALUE,ATTR_DIMMED,1);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_SEQVALUE,	 ATTR_DIMMED,1); 
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_RANGEVALUE,ATTR_DIMMED,1); 
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DECORATION_7,	ATTR_VISIBLE, 1); 
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTINCH,		ATTR_VISIBLE, 1); 
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_WHEELDIM,		ATTR_VISIBLE, 1); 
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_WHEELDIM,		ATTR_DIMMED, 0);
			
		cell.x = 3 ; 
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
		token = strtok(sCellValue, "|");
		token[strlen(token)-1]=NULL;
		SetCtrlVal(panelHandle - 3,EXPRESULTS_LABEL1,token);
		token = strtok(NULL, "|");
		token[strlen(token)-1]=NULL;
		MoveString(token);
		SetCtrlVal(panelHandle - 3,EXPRESULTS_LABEL2,token);
		
	/*	cell.x = 4 ;
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
		SetCtrlVal(panelHandle - 3,EXPRESULTS_PARAMETERSDATA,0);  
			
		cell.x = 5 ;
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
		SetCtrlVal(panelHandle - 3,EXPRESULTS_FIELDCHECK,0);*/ 

		cell.x = 6 ;
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
		sCellValue[strlen(sCellValue)-1]=NULL;
		//printf("sCellValue = %s\n",sCellValue);
		SetCtrlVal(panelHandle - 3,EXPRESULTS_WHEELDIM,sCellValue); 
		
		cell.x = 7 ;
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
		if (strcmp(sCellValue,"0;")==0)
		{
		cell.x = 8 ;
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE1,ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE2,ATTR_DIMMED,1);
		SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_BTNTOLVAL,0); 
		SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_BTNTOLPRCT,1); 
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
		sCellValue[strlen(sCellValue)-1]=NULL;
	    SetCtrlVal(panelHandle - 3,EXPRESULTS_TOLERENCE1,sCellValue);
		}
		else
		{
		sCellValue[strlen(sCellValue)-1]=NULL;
		SetCtrlVal(panelHandle - 3,EXPRESULTS_TOLERENCE2,sCellValue);
		}
		
		commandExpResult = 8; 
	   
	}  
	else if (strcmp(sCellValue, "CheckNoRF") ==0) 
	{		
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_LABEL1,			ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_LABEL2,			ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_VALUE,			ATTR_DIMMED,1);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TXTUNITLABEL,		ATTR_DIMMED,1);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDVAL,			ATTR_DIMMED,1);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DELVALUE,			ATTR_DIMMED,1);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG,			ATTR_DIMMED,1);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE1,		ATTR_DIMMED,1);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE2,		ATTR_DIMMED,1);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDTOL,			ATTR_DIMMED,1);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDTOL_2,			ATTR_DIMMED,1);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDEXP,			ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,		ATTR_DIMMED,1);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_VALUEFC,			ATTR_DIMMED,1);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA,	ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BTNTOLVAL,		ATTR_DIMMED,1); 
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BTNTOLPRCT,		ATTR_DIMMED,1);
		SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_TXTUNITBODY, ""); 
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DECORATION_7,	ATTR_VISIBLE, 0); 
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTINCH,		ATTR_VISIBLE, 0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_WHEELDIM,		ATTR_VISIBLE, 0); 
	   
		cell.x = 3 ; 
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
		token = strtok(sCellValue, "|");
		token[strlen(token)-1]=NULL; 
		SetCtrlVal(panelHandle - 3,EXPRESULTS_LABEL1,token);
		token = strtok(NULL, "|");
		token[strlen(token)-1]=NULL;
		MoveString(token);
		SetCtrlVal(panelHandle - 3,EXPRESULTS_LABEL2,token);
		
		cell.x = 4 ;
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
		SetCtrlVal(panelHandle - 3,EXPRESULTS_PARAMETERSDATA,0);
			
		cell.x = 5 ;
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
		SetCtrlVal(panelHandle - 3,EXPRESULTS_FIELDCHECK,0);
		
		commandExpResult = 9;
	}   
	else if (strcmp(sCellValue, "CheckTimingFirstRF") ==0) 
	{		
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_VALUEFC,			ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_LABEL1,			ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_LABEL2,			ATTR_DIMMED,1);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_VALUE,			ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TXTUNITLABEL,		ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDVAL,			ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DELVALUE,			ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG,			ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE1,		ATTR_DIMMED,1);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE2,		ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDTOL,			ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDTOL_2,			ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDEXP,			ATTR_DIMMED,0); 
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA,	ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,		ATTR_DIMMED,1);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_VALUEFC,			ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BTNTOLVAL,		ATTR_DIMMED,0); 
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BTNTOLPRCT,		ATTR_DIMMED,0); 
		SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_TXTUNITBODY, "[s]"); 
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_FIXEDVALUE,ATTR_DIMMED,1);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_SEQVALUE,	 ATTR_DIMMED,1);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_RANGEVALUE,ATTR_DIMMED,0); 
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DECORATION_7,	ATTR_VISIBLE, 0); 
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTINCH,		ATTR_VISIBLE, 0); 
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_WHEELDIM,		ATTR_VISIBLE, 0); 
	 
		cell.x = 2 ;
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
	    SetCtrlVal(panelHandle - 3,EXPRESULTS_VALUEFC,sCellValue); 
		
		cell.x = 3 ; 
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
		sCellValue[strlen(sCellValue)-1]=NULL; 
		SetCtrlVal(panelHandle - 3,EXPRESULTS_LABEL1,sCellValue);
		
		
		cell.x = 4 ;
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
	    SetCtrlVal(panelHandle - 3,EXPRESULTS_PARAMETERSDATA,0); 

		cell.x = 5 ;
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
	    SetCtrlVal(panelHandle - 3,EXPRESULTS_FIELDCHECK,0); 

		cell.x = 6 ;
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
		sCellValue[strlen(sCellValue)-1]=NULL;
	    SetCtrlVal(panelHandle - 3,EXPRESULTS_VALUE,sCellValue);

		cell.x = 7 ;
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
		if (strcmp(sCellValue,"0;")==0)
		{
		cell.x = 8 ;
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE1,ATTR_DIMMED,0);
		SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE2,ATTR_DIMMED,1);
		SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_BTNTOLVAL,0); 
		SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_BTNTOLPRCT,1); 
		GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
		sCellValue[strlen(sCellValue)-1]=NULL;
	    SetCtrlVal(panelHandle - 3,EXPRESULTS_TOLERENCE1,sCellValue);
		}
		else
		{
		sCellValue[strlen(sCellValue)-1]=NULL;
		SetCtrlVal(panelHandle - 3,EXPRESULTS_TOLERENCE2,sCellValue);
		}
		
		commandExpResult = 10;
	}   	
		
}

	   





















//***********************************************************************************************
// Modif - Carolina 02/2019
// This table informs the user about the current script execution
// Not finished
//***********************************************************************************************
/*int CVICALLBACK 
SCRIPTS_DETAILS_2 (int panel, int control, int event,
								   void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{
		case EVENT_COMMIT:

			break;
		case EVENT_RIGHT_CLICK:

			break;
	}
	return 0;
} */

//***********************************************************************************************
// Modif  MARVYN
// This function puts color on the table (test script)
//***********************************************************************************************
void couleurLigne(int numeroLigne, int GiPanel)
{
	Point cell;
	cell.x=8;
	cell.y=numeroLigne;

	//variables for test script
	char conditionName[20]="";

	char condition1[20]="Pre";
	char condition2[20]="Script";
	char condition3[20]="Post";

	int parameterCompare1;
	int parameterCompare2;
	int parameterCompare3;

	//char condition4[20]="Pre";
	//char condition5[20]="Script";
	//char condition6[20]="Post";

	//int parameterCompare4;
	//int parameterCompare5;
	//int parameterCompare6;
	//printf("cellx = %d et celly = %d\n",cell.x,cell.y);
	
	GetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS,cell, conditionName);  //Test script table

	parameterCompare1=strcmp(conditionName,condition1);

	if(parameterCompare1 == 0)	   //green
	{
		SetTableCellRangeAttribute (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, VAL_TABLE_ROW_RANGE (numeroLigne),
									ATTR_TEXT_BGCOLOR, 0xA9F79DL);
	}

	parameterCompare2=strcmp(conditionName,condition2);

	if(parameterCompare2 == 0)  //white
	{
		SetTableCellRangeAttribute (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, VAL_TABLE_ROW_RANGE (numeroLigne),
									ATTR_TEXT_BGCOLOR, VAL_WHITE);

	}

	parameterCompare3=strcmp(conditionName,condition3);

	if(parameterCompare3 == 0)   //blue
	{
		SetTableCellRangeAttribute (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, VAL_TABLE_ROW_RANGE (numeroLigne),
									ATTR_TEXT_BGCOLOR, 0x77BBFFL);
	}
}
	//MARVYN 23/06/2021
void couleurSelection(int numeroLigne,int control, int GiPanel, int color)
{
	Point cell;
	cell.x=8;
	cell.y=numeroLigne;
	if (numeroLigne != 0)
	{
	SetTableCellRangeAttribute (GiPanel, control, VAL_TABLE_ROW_RANGE (numeroLigne),ATTR_TEXT_BGCOLOR, color);
	}
	else 
	{
		return 0;
	}

}

//***********************************************************************************************
// fonction insert step
// This function is not being used
//***********************************************************************************************
void insert_steps(int numeroLigne, int GiPanel)
{

	char range[30];						// Point exel ex: A2 , en chaine de caractère
	char cond_end[100]="";				// Parameter du fichier excel en chaine de caractère
	char parameter[100]="";             //char *parameterCompare=NULL;		// chaine de caractère qui cherche la fin des parametres sur le fichier excel
	int boucle;
	int end=1;
	int parameterCompare;



	// Ouverture du panel
	GiPopupAdd2 = LoadPanel (0, "IhmModes.uir", PANEL_ADD);
	DisplayPanel(GiPopupAdd2);
	SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_DECORATION_2,ATTR_VISIBLE,1);
	SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_SCRIPT_ADD,ATTR_VISIBLE,0);
	SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_BOUTON_INSERT,ATTR_VISIBLE,1);

	// Ouverture de la database et récuprération des paramètres
	// La boucle récupère les parametres du fichier excel (dynamique)



	
	for(boucle=1; end==1; boucle++)
	{
		sprintf(range,"A%d",boucle+1);
		ExcelRpt_GetCellValue (worksheetHandledata, range, CAVT_CSTRING, parameter);
		//parameterCompare=strchr(parameter,'end');
		parameterCompare=strcmp(parameter,cond_end);	// compare les deux chaine de caractère

		if(parameterCompare == 0)
		{
			end=0;
		}
		else
		{
			InsertListItem (GiPopupAdd2,PANEL_ADD_PARAMETERS,boucle,parameter,boucle);
		}
	} 

	//end=1;
	//ExcelRpt_GetWorksheetFromIndex(workbookHandle, 1, &worksheetHandle4);


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Récupération fichier ANum


#define TAILLE_MAX 1000

	FILE* fichier = NULL;
	char chaine[TAILLE_MAX] = "";
	int cpt;//, cpt2, cpt3;
	int nb_frames_lf=0;
	//int nb_frames_rf=0;
	int longueurChaine;



	fichier = fopen(GsAnumCfgFile, "r");		  // Ouverture du fichier

	if (fichier != NULL)
	{
		// récupération du nombre de frame
		for(cpt=0; cpt<100; cpt++)
		{
			fgets(chaine, TAILLE_MAX, fichier);
			if( chaine[0]=='[' &&  chaine[1]=='L'  &&  chaine[2]=='F'  &&  chaine[3]=='C' )
			{
				fgets(chaine, TAILLE_MAX, fichier);
				cpt=100;
			}
		}

		longueurChaine=strlen(chaine);// calcul de la longueur de la chaine
		if(longueurChaine== 11)
		{
			nb_frames_lf=chaine[9]-48;   // calcul le nb de frame pour une chaine a un chiffre
		}
		else if (longueurChaine== 12)
		{
			nb_frames_lf=(chaine[9]-48)*10+chaine[10]-48;  // calcul du nb de frame pour une chaine a deux chiffres
		}

		//	printf("Nombre de Frame LF: %d \n", nb_frames_lf);
		//  printf("\n %d", longueurChaine);


		// récupération des données dans les frames
		for(cpt=0; cpt<nb_frames_lf; cpt++) 			// parcours des lignes en fonction du nombres de frames
		{
			fgets(chaine, TAILLE_MAX, fichier);


			int curseur_chaine;
			int compteur_chaine;

			// récupération "Name"
			for(curseur_chaine=0; curseur_chaine< strlen(chaine); curseur_chaine++)
			{
				if( chaine[curseur_chaine]=='N' &&  chaine[curseur_chaine+1]=='a'  &&  chaine[curseur_chaine+2]=='m'  &&  chaine[curseur_chaine+3]=='e' )
				{
					char frame_name[50]="";

					for(compteur_chaine=0; chaine[curseur_chaine + compteur_chaine + 5]!='\¬'; compteur_chaine++)
					{
						frame_name[compteur_chaine]=chaine[curseur_chaine + compteur_chaine + 5];
					}

					InsertListItem (GiPopupAdd2,PANEL_ADD_LFDNAME,cpt+1,frame_name,frame_name);
					//	printf("\n Name: %s", frame_name );
				}
			}
		}




		//AJOUT
		// Fin modif ANum
////////////////////////////////////////////////////////////////////////////////////////////////////////


	}

	//	}




}

//***********************************************************************************************
// Modif - Carolina 2019
// Add expected results
//***********************************************************************************************
int add_expected_results()
{
	//char range[30];
	//char cond_end[100]="";
	//char ParameterName[100]="";
	//char UnitParameter[10];
	//int boucle=0;
	//int end=1;
	//int parameterCompare;

	int endid=1;
	int boucleid = 0;
	int parameterCompare;
	char rangeid[30];
	char wuid[100]="";
	char cond_endid[100]="";
	
	
	
	if ((GiExpectedResultsPanel = LoadPanel (0, "IhmModes.uir", EXPRESULTS)) < 0)
		return -1;
	SetPanelAttribute (GiExpectedResultsPanel, ATTR_CLOSE_CTRL, EXPRESULTS_OKBUTTON);
	DisplayPanel(GiExpectedResultsPanel);


	for(int row=1; row <= nbParameter; row++)
	{
		InsertListItem (GiExpectedResultsPanel, EXPRESULTS_SELECTPARAM,row,myParameterTab[row-1].name,row);
	}


	// Modif MaximePAGES 13/08/2020 - WU ID selection in IHM ************
	InsertListItem (GiExpectedResultsPanel,EXPRESULTS_WUIDEXP,0,"ID ind",0);
	for(boucleid=1; endid==1; boucleid++) //
	{

		sprintf(rangeid,"O%d",boucleid+1);  //start with O2
		ExcelRpt_GetCellValue (worksheetHandledata1, rangeid, CAVT_CSTRING, wuid);

		//stop condition
		parameterCompare = strcmp(wuid,cond_endid);	// compare les deux chaine de caractère

		if(parameterCompare == 0) //when parameter is ""
		{
			endid=0;
		}
		else
		{
			InsertListItem (GiExpectedResultsPanel,EXPRESULTS_WUIDEXP,boucleid,wuid,boucleid);
		}
	}

	endid=1;
	//**********************************************************************

	 for (int j=0; j<indexInterfaceStruct;j++)
	{
		//printf("Interfaces[j].name = %s\n",Interfaces[j].name);
		InsertListItem (GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA,j+1,Interfaces[j].name,j+1);
	}	

	return 0;
}


// fonction ajout, ouverture du panel ADD
void add_menu()
{

						// Point exel ex: A2 , en chaine de caractère
				// Parameter du fichier excel en chaine de caractère
	            //char *parameterCompare=NULL;		// chaine de caractère qui cherche la fin des parametres sur le fichier excel


	int endid=1;
	int boucleid = 0;
	int parameterCompare;
	char rangeid[30];
	char wuid[100]="";
	char cond_endid[100]="";


	if(iAnumLoad!= 1 && iDataBaseLoad!=1)
	{
		MessagePopup ("Warning", "No ANum Config and No DataBase Selected!");
	}
	else if(iAnumLoad== 1 && iDataBaseLoad!=1)
	{
		MessagePopup ("Warning", "No DataBase Selected!");
	}
	else if(iAnumLoad!= 1 && iDataBaseLoad==1)
	{
		MessagePopup ("Warning", "No ANum Config Selected!");
	}
	else
	{

		// Ouverture du panel

		GiPopupAdd2 = LoadPanel (0, "IhmModes.uir", PANEL_ADD);   // Load Panel_add problem
		DisplayPanel(GiPopupAdd2);
		SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_DECORATION_2,ATTR_VISIBLE,1);
		SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_SCRIPT_ADD,ATTR_VISIBLE,1);
		SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_BOUTON_INSERT,ATTR_VISIBLE,0);
		
		InsertListItem (GiPopupAdd2,PANEL_ADD_WUID,0,"",0);
		//InsertListItem (GiPopupAdd2,PANEL_ADD_PARAMETERS,0,"",0);

		//Modif MaximePAGES 06/08/2020
		for(int row=1; row <= nbParameter; row++)
		{
			InsertListItem (GiPopupAdd2,PANEL_ADD_PARAMETERS,row,myParameterTab[row-1].name,row);
		}
		
		

		 (GiPopupAdd2,PANEL_ADD_WUID,0,"ID ind",0);
		for(boucleid=1; endid==1; boucleid++) //
		{

			sprintf(rangeid,"O%d",boucleid+1);  //start with O2
			ExcelRpt_GetCellValue (worksheetHandledata1, rangeid, CAVT_CSTRING, wuid);

			//stop condition
			parameterCompare = strcmp(wuid,cond_endid);	// compare les deux chaine de caractère

			if(parameterCompare == 0) //when parameter is ""
			{
				endid=0;
			}
			else
			{
				InsertListItem (GiPopupAdd2,PANEL_ADD_WUID,boucleid,wuid,boucleid);
			}
		}

		endid=1;
		
		
		
		
		
		
		


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Récupération fichier ANum

#define TAILLE_MAX 1000
		FILE* fichier = NULL;
		char chaine[TAILLE_MAX] = "";
		int nb_frames_lf=0;
		//int nb_frames_rf=0;
		int longueurChaine;

		// Ouverture du fichier
		fichier = fopen(GsAnumCfgFile, "r");

		if (fichier != NULL)
		{
			// récupération du nombre de frame
			for(int cpt=0; cpt<100; cpt++)
			{
				fgets(chaine, TAILLE_MAX, fichier);
				if( chaine[0]=='[' &&  chaine[1]=='L'  &&  chaine[2]=='F'  &&  chaine[3]=='C' )
				{
					fgets(chaine, TAILLE_MAX, fichier);
					cpt=100;
				}
			}

			// calcul de la longueur de la chaine
			longueurChaine=strlen(chaine);

			if(longueurChaine== 11)
			{
				nb_frames_lf=chaine[9]-48;   // calcul le nb de frame pour une chaine a un chiffre
			}
			else if (longueurChaine== 12)
			{
				nb_frames_lf=(chaine[9]-48)*10+chaine[10]-48;  // calcul du nb de frame pour une chaine a deux chiffres
			}

			// récupération des données dans les frames
			for(cpt=0; cpt<nb_frames_lf; cpt++) 			// parcours des lignes en fonction du nombres de frames
			{
				fgets(chaine, TAILLE_MAX, fichier);
				int curseur_chaine;
				int compteur_chaine;

				// récupération "Name"
				for(curseur_chaine=0; curseur_chaine< strlen(chaine); curseur_chaine++)
				{
					if( chaine[curseur_chaine]=='N' &&  chaine[curseur_chaine+1]=='a'
							&&  chaine[curseur_chaine+2]=='m'
							&&  chaine[curseur_chaine+3]=='e' )
					{
						char frame_name[50]="";
						for(compteur_chaine=0; chaine[curseur_chaine + compteur_chaine + 5]!='\¬'; compteur_chaine++)
						{
							frame_name[compteur_chaine]=chaine[curseur_chaine + compteur_chaine + 5];
						}
						InsertListItem (GiPopupAdd2,PANEL_ADD_LFDNAME,cpt+1,frame_name,frame_name);
					}
				}
			}
			// Fin modif ANum
////////////////////////////////////////////////////////////////////////////////////////////////////////
			///////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
		}
	}
}


int CVICALLBACK wuid_fonction (int panel, int control, int event,
							   void *callbackData, int eventData1, int eventData2)
{
	int itemValue=10;
	char range[30];
	char valueParameter[100];
	//char unitParameter[100];
	char valueDefault[100]=" ";

	switch (event)
	{
		case EVENT_VAL_CHANGED:

			//SetCtrlVal(panel,PANEL_ADD_WUID_VALUE,"");
			GetCtrlVal(panel,PANEL_ADD_WUID, &itemValue);

			if (itemValue==0)
			{
				SetCtrlVal(panel,PANEL_ADD_WUID_VALUE,valueDefault);
			}
			else
			{
				sprintf(range,"P%d",itemValue+1);
				ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, valueParameter); //ligne peut être réutilisable pour récupérer l'ID, faire un if ID1 =0 ID1Diag = 1 ... ID2 = 3 et utiliser cette ligne pour récupérer la valeur !
				SetCtrlVal(panel,PANEL_ADD_WUID_VALUE,valueParameter);
			}
			break;
	}
	return 0;
}


int CVICALLBACK Insert_Parameter1 (int panel, int control, int event,
								   void *callbackData, int eventData1, int eventData2)
{


	switch (event)
	{
		case EVENT_COMMIT:

			//make visible
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT1,ATTR_VISIBLE, 1); //from time
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT1,ATTR_DIMMED, 0); //from time
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT2,ATTR_VISIBLE, 0); //from duration
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT2,ATTR_DIMMED, 1); //from time

			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_TIME,ATTR_VISIBLE, 1); //from time
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_DURATION,ATTR_VISIBLE, 0); //from duration

			dimmedNumericKeypadForScript(0);

			break;
	}
	return 0;
}

int CVICALLBACK Insert_Parameter2 (int panel, int control, int event,
								   void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{
		case EVENT_COMMIT:

			//make visible
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT1,ATTR_VISIBLE, 0); //from time
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT1,ATTR_DIMMED, 1); //from time
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT2,ATTR_VISIBLE, 1); //from duration
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT2,ATTR_DIMMED, 0); //from time

			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_TIME,ATTR_VISIBLE, 0); //from time
			SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_DURATION,ATTR_VISIBLE, 1); //from duration

			dimmedNumericKeypadForScript(0);

			break;
	}
	return 0;
}


// affichage du WU ID si LFData en individual

int CVICALLBACK lfdname (int panel, int control, int event,
						 void *callbackData, int eventData1, int eventData2)
{
	char nameMLF[100]="";
	int boucle;
	int wuid =0;


	switch (event)
	{
		case EVENT_VAL_CHANGED:

			GetCtrlVal(panel,PANEL_ADD_LFDNAME, nameMLF);
		//	printf("nameMLF = %s", nameMLF);

			for(boucle=0; boucle < strlen(nameMLF); boucle++)
			{
				if(nameMLF[boucle]=='I' && nameMLF[boucle+1]=='n' && nameMLF[boucle+2]=='d' )
				{
					wuid=1;
				}
			}
			if(wuid==1)
			{
				//SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_DECORATION_WUID,ATTR_VISIBLE,0);
				SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID,ATTR_DIMMED, 0);
				SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID_VALUE,ATTR_DIMMED, 0);
			}
			else
			{
				//SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_DECORATION_WUID,ATTR_VISIBLE,1);
				SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID,ATTR_DIMMED, 1);
				SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_WUID_VALUE,ATTR_DIMMED, 1);
			}

			break;
	}
	return 0;
}


//***********************************************************
// AJOUT Maxime PAGES - 10/06/2020


int CVICALLBACK lfdnbframe (int panel, int control, int event,
							void *callbackData, int eventData1, int eventData2)
{
	char nameMLF[100]="";




	switch (event)
	{
		case EVENT_VAL_CHANGED:

			GetCtrlVal(panel,PANEL_ADD_LFDNBFRAME, nameMLF);
		//	printf("nameMLF = %s", nameMLF); 

			

			break;
	}
	return 0;
}









///////////////////////


// DEBUT ALGO - mode automatique
//******************************************************************************
// int GstIhmCycles(int iPanel, int iEtatCyclesVitPres, int iEtatCycleVitesse,
//					int iEtatCyclePression)
//******************************************************************************
//	- int iPanel 				: Handle du panel
//	  int iEtatCyclesVitPres	: Etat du cycle de vitesse/pression
//	  int iEtatCycleVitesse		: Etat du cycle de vitesse
//	  int iEtatCyclePression	: Etat du cycle de pression
//
//  - Configuration de l'IHM en fonction du déroulement des cycles //int iGraphPanel
//
//  - 0 si OK, sinon erreur
//******************************************************************************
// FIN ALGO
int GstIhmCycles(int iPanel, int iEtatCyclesVitPres, int iEtatCycleVitesse, int iEtatCyclePression)
{
	if ((iEtatCycleVitesse == iEN_COURS) || (iEtatCyclePression == iEN_COURS))
	{
		//SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_CONFIG, 			ATTR_DIMMED, 1);	//Modif MaximePAGES 09/09/2020
		//SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_MODE_AUTOMATIQUE, ATTR_DIMMED, 1);  //Modif MaximePAGES 09/09/2020
		//SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_MODE_MANUEL, 		ATTR_DIMMED, 1);//Modif MaximePAGES 09/09/2020
		SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_QUITTER, 			ATTR_DIMMED, 1);
		//SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_MODE_RUN, 		ATTR_DIMMED, 1);  //Modif MaximePAGES 09/09/2020

		if (iEtatCyclesVitPres == iEN_COURS)  //mode manual
		{
			SetCtrlAttribute (iPanel, PANEL_MODE_BUTT_START_VIT_PRESS, 	ATTR_DIMMED, 0);
			// Traduction du bouton de Départ cycle vitesses et pressions
			SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VIT_PRESS, ATTR_LABEL_TEXT, sBOUTON_STOP_VIT_PRES);

		}
		else
		{
			SetCtrlAttribute (iPanel, PANEL_MODE_BUTT_START_VIT_PRESS, 	ATTR_DIMMED, 1);
			// Traduction du bouton de Départ cycle vitesses et pressions
			if ((GetGiMode() == iMODE_AUTOMATIQUE))
				SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VIT_PRESS, ATTR_LABEL_TEXT, sBOUTON_DEP_VIT_PRES);
			else
				SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VIT_PRESS, ATTR_LABEL_TEXT, sBOUTON_VALID_VIT_PRES);
		}
	}
	else
	{
		//SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_CONFIG, 			ATTR_DIMMED, 0);  //Modif MaximePAGES 09/09/2020
		/*
		if ((GetGiMode() == iMODE_AUTOMATIQUE))
			//SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_MODE_MANUEL, 		ATTR_DIMMED, 0);   //Modif MaximePAGES 09/09/2020
		else
			//SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_MODE_AUTOMATIQUE, ATTR_DIMMED, 0);
			//SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_MODE_AUTOMATIQUE, ATTR_DIMMED, 1);//Modif MaximePAGES 09/09/2020
	   */
		SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_QUITTER, 			ATTR_DIMMED, 0);

		// Traduction du bouton de Départ cycle vitesses et pressions
		if ((GetGiMode() == iMODE_AUTOMATIQUE))
			SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VIT_PRESS, ATTR_LABEL_TEXT, sBOUTON_DEP_VIT_PRES);
		else
			SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VIT_PRESS, ATTR_LABEL_TEXT, sBOUTON_VALID_VIT_PRES);
	}


	if (iEtatCyclesVitPres == iEN_COURS)
	{
		if ((iEtatCycleVitesse == iARRET) && (iEtatCyclePression == iARRET))
		{
			// Traduction du bouton de Départ cycle vitesses et pressions
			if ((GetGiMode() == iMODE_AUTOMATIQUE))
				SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VIT_PRESS, ATTR_LABEL_TEXT, sBOUTON_DEP_VIT_PRES);
			else
				SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VIT_PRESS, ATTR_LABEL_TEXT, sBOUTON_VALID_VIT_PRES);

			SetGiStartVitPress(0);
			SetCtrlAttribute (iPanel, PANEL_MODE_BUTT_START_VITESSE, 	ATTR_DIMMED, 0);
			SetCtrlAttribute (iPanel, PANEL_MODE_BUT_START_PRESSION, 	ATTR_DIMMED, 0);
		}
		else
		{
			if (iEtatCycleVitesse == iARRET)
				SetCtrlAttribute (iPanel, PANEL_MODE_BUTT_START_VITESSE, 	ATTR_DIMMED, 1);

			if (iEtatCyclePression == iARRET)
				SetCtrlAttribute (iPanel, PANEL_MODE_BUT_START_PRESSION, 	ATTR_DIMMED, 1);
		}
	}

	//--------- VITESSE -----------------
	if (iEtatCycleVitesse == iEN_COURS)
	{
		SetCtrlAttribute (iPanel, PANEL_MODE_TABLE_VITESSES, 		ATTR_CTRL_MODE, VAL_INDICATOR);
		SetCtrlAttribute (iPanel, PANEL_MODE_NBE_CYCLES_VITESSE, 	ATTR_CTRL_MODE, VAL_INDICATOR);
		SetCtrlAttribute (iPanel, PANEL_MODE_TABLE_PRESSION, 		ATTR_CTRL_MODE, VAL_INDICATOR);
		SetCtrlAttribute (iPanel, PANEL_MODE_NBE_CYCLES_PRESSION, 	ATTR_CTRL_MODE, VAL_INDICATOR);

		// Traduction du bouton de Départ cycle vitesses
		SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VITESSE, ATTR_LABEL_TEXT, sBOUTON_STOP_VIT);
	}
	else
	{
		if ((iEtatCycleVitesse != iEN_COURS) && (iEtatCyclePression != iEN_COURS))
		{
			SetCtrlAttribute (iPanel, PANEL_MODE_TABLE_VITESSES, 		ATTR_CTRL_MODE, VAL_HOT);
			SetCtrlAttribute (iPanel, PANEL_MODE_NBE_CYCLES_VITESSE, 	ATTR_CTRL_MODE, VAL_HOT);
			SetCtrlAttribute (iPanel, PANEL_MODE_TABLE_PRESSION, 		ATTR_CTRL_MODE, VAL_HOT);
			SetCtrlAttribute (iPanel, PANEL_MODE_NBE_CYCLES_PRESSION,	ATTR_CTRL_MODE, VAL_HOT);
		}
		// Traduction du bouton de Départ cycle vitesses
		if ((GetGiMode() == iMODE_AUTOMATIQUE))
			SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VITESSE, ATTR_LABEL_TEXT, sBOUTON_DEP_VIT);
		else
			SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VITESSE, ATTR_LABEL_TEXT, sBOUTON_VALID_VIT);
	}

	//--------- PRESSION -----------------
	if (iEtatCyclePression == iEN_COURS)
	{
		SetCtrlAttribute (iPanel, PANEL_MODE_TABLE_PRESSION, 		ATTR_CTRL_MODE, VAL_INDICATOR);
		SetCtrlAttribute (iPanel, PANEL_MODE_NBE_CYCLES_PRESSION, 	ATTR_CTRL_MODE, VAL_INDICATOR);
		SetCtrlAttribute (iPanel, PANEL_MODE_TABLE_VITESSES, 		ATTR_CTRL_MODE, VAL_INDICATOR);
		SetCtrlAttribute (iPanel, PANEL_MODE_NBE_CYCLES_VITESSE, 	ATTR_CTRL_MODE, VAL_INDICATOR);

		// Traduction du bouton de Départ cycle pressions
		SetCtrlAttribute (GiPanel, PANEL_MODE_BUT_START_PRESSION, ATTR_LABEL_TEXT, sBOUTON_STOP_PRES);
	}
	else
	{
		if ((iEtatCycleVitesse != iEN_COURS) && (iEtatCyclePression != iEN_COURS))
		{
			SetCtrlAttribute (iPanel, PANEL_MODE_TABLE_VITESSES, 		ATTR_CTRL_MODE, VAL_HOT);
			SetCtrlAttribute (iPanel, PANEL_MODE_NBE_CYCLES_VITESSE, 	ATTR_CTRL_MODE, VAL_HOT);
			SetCtrlAttribute (iPanel, PANEL_MODE_TABLE_PRESSION, 		ATTR_CTRL_MODE, VAL_HOT);
			SetCtrlAttribute (iPanel, PANEL_MODE_NBE_CYCLES_PRESSION,	ATTR_CTRL_MODE, VAL_HOT);
		}
		// Traduction du bouton de Départ cycle pressions
		if ((GetGiMode() == iMODE_AUTOMATIQUE))
			SetCtrlAttribute (GiPanel, PANEL_MODE_BUT_START_PRESSION, ATTR_LABEL_TEXT, sBOUTON_DEP_PRES);
		else
			SetCtrlAttribute (GiPanel, PANEL_MODE_BUT_START_PRESSION, ATTR_LABEL_TEXT, sBOUTON_VALID_PRES);
	}

	return 0; //OK
}

//initialize the Anum Interface for automatic mode
void InitAnum(BOOL isRun)
{
	char sAnumPath[MAX_PATHNAME_LEN];
	HANDLE  hDLL=NULL;
	TAPI      myFUnc=0;
	//struct    TAPILFRF sAPILFRF;
	//unsigned char cAPI;
	//char aCommand[128];


	if(isRun)
	{
		if(!GbAnumInit)
		{
			GetProjectDir (GsPathApplication);
			sprintf (sAnumPath, "%s\\%s", GsPathApplication, sANUM_DLL);
			//sprintf (sAnumPath, "%s\\%s", GsPathApplication, "IPC.dll");
			ModesAddMessageZM("Init ANUM interface...");
			hDLL=LoadLibrary(sAnumPath);
			//hDLL=LoadLibrary("API.dll");
			if (hDLL==NULL)
			{
				ModesAddMessageZM("Failed to load ANUM IPC.dll!");
				GbAnumInit=0;
				return;
			}
			SetGiAnumHdll(hDLL);
			// Get function
			myFUnc=(TAPI)GetProcAddress(hDLL, "API");
			if (myFUnc==NULL)
			{
				ModesAddMessageZM("Failed to load function from IPC.dll!");
				GbAnumInit=0;
				return;
			}
			SetGiAnumApi(myFUnc);
			/*
						sprintf (sAnumPath, "%s\\%s\\%s", GsPathApplication,"Config", sANUM_CFG);
						strcpy(aCommand, "LOAD");
						strcpy(sAPILFRF.StringIn1, sAnumPath);
						cAPI=myFUnc(aCommand, (void*)&sAPILFRF);

						if (cAPI!=0)
						{
							ModesAddMessageZM("Failed to load ANUM cfg!");
							GbAnumInit=0;
							return;
						}

						 strcpy(aCommand, "LFPWR");
						 sAPILFRF.ByteIn1=100;
						 cAPI=myFUnc(aCommand, (void*)&sAPILFRF);
						*/
			ModesAddMessageZM("ANUM interface OK!");
			GbAnumInit=1;
		}
	}
	else
	{
		if(GbAnumInit)
		{
			FreeLibrary(hDLL);
			GbAnumInit=0;
		}
	}

}

// DEBUT ALGO
//******************************************************************************
// void DisplayMode(int iPanel, int iMode)
//******************************************************************************
//	- int iPanel: Handle du panel
//	  int iMode : Mode manuel ou automatique
//
//  - Configuration de l'affichage de l'écran en fonction du mode (automatique/manuel)
//
//  - Aucun
//******************************************************************************
// FIN ALGO
void DisplayMode(int iPanel, int iMode)
{
#define iTOP_UNIT_VITESSE_MANUEL	176
#define iTOP_UNIT_PRESSION_MANUEL	444
#define iTOP_UNIT_VITESSE_AUTO		256
#define iTOP_UNIT_PRESSION_AUTO		520
	int iErr;
	BOOL bDimmed;
	BOOL bVisible;

	SetGbQuitter(0);

	// Effacement des graphiques
	DeleteGraphPlot (iPanel, PANEL_MODE_GRAPHE_VITESSE, -1, VAL_IMMEDIATE_DRAW);
	DeleteGraphPlot (iPanel, PANEL_MODE_GRAPHE_PRESSION, -1, VAL_IMMEDIATE_DRAW);
	DeleteGraphPlot (GiGraphPanel, PANELGRAPH_GRAPH, -1, VAL_IMMEDIATE_DRAW);

	switch (iMode)
	{
			// Passage en mode manuel
		case iMODE_MANUEL:
			bDimmed 	= 1;
			bVisible 	= 0;

			// Changement des libellés des boutons de démarrage des cycles
			iErr = SetCtrlAttribute (iPanel, PANEL_MODE_BUTT_START_VITESSE, ATTR_LABEL_TEXT, sBOUTON_VALID_VIT);
			// Traduction du bouton de Départ cycle pressions
			iErr = SetCtrlAttribute (iPanel, PANEL_MODE_BUT_START_PRESSION, ATTR_LABEL_TEXT, sBOUTON_VALID_PRES);
			// Traduction du bouton de Départ cycle vitesses et pressions
			iErr = SetCtrlAttribute (iPanel, PANEL_MODE_BUTT_START_VIT_PRESS, ATTR_LABEL_TEXT, sBOUTON_VALID_VIT_PRES);

			// Déplacement du contrôle de choix d'unités
			iErr = SetCtrlAttribute (iPanel, PANEL_MODE_UNIT_VITESSE, 	 ATTR_TOP, 	iTOP_UNIT_VITESSE_MANUEL);
			iErr = SetCtrlAttribute (iPanel, PANEL_MODE_UNIT_PRESSION,	 ATTR_TOP, 	iTOP_UNIT_PRESSION_MANUEL);

			//Oprion cfg
			//SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_CONFIG_CHARGER, 	ATTR_DIMMED, 	bDimmed);//Modif MaximePAGES 09/09/2020
			//SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_CONFIG_ENREGISTRER, ATTR_DIMMED, 	bDimmed); //Modif MaximePAGES 09/09/2020
			//SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_CONFIG_SEE_CURR_CYCLE, ATTR_DIMMED, 	bDimmed);  //Modif MaximePAGES 09/09/2020
			//SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_MODE_MANUEL, 		ATTR_DIMMED, 	!bDimmed); //Modif MaximePAGES 09/09/2020
			//SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_MODE_AUTOMATIQUE, ATTR_DIMMED,	!bDimmed);  //Modif MaximePAGES 09/09/2020

			//SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_MODE_RUN, ATTR_DIMMED,	0);   //Modif MaximePAGES 09/09/2020
			//SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_CONFIG_ITEM_LOAD_CONF, 	ATTR_DIMMED, 	1);	   //Modif MaximePAGES 09/09/2020
			//SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_CONFIG_ITEM_SAVE_CONF, 	ATTR_DIMMED, 	1);	   //Modif MaximePAGES 09/09/2020
			//SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_CONFIG_ITEM_LOAD_ANUM_CFG, 	ATTR_DIMMED, 	1); //Modif MaximePAGES 09/09/2020
			//SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_CONFIG_ITEM_LOAD_DATABASE, 	ATTR_DIMMED, 	1);   //Modif MaximePAGES 09/09/2020
			//SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_CONFIG_ITEM_SET_LOG_DIR, 	ATTR_DIMMED, 	1);   //Modif MaximePAGES 09/09/2020

			SetCtrlAttribute (iPanel, PANEL_MODE_TABLE_VITESSES, ATTR_VISIBLE, 			bVisible);
			SetCtrlAttribute (iPanel, PANEL_MODE_TABLE_PRESSION, ATTR_VISIBLE, 			bVisible);

			SetCtrlAttribute (iPanel, PANEL_MODE_NBE_CYCLES_PRESSION, ATTR_VISIBLE, 	bVisible);
			SetCtrlAttribute (iPanel, PANEL_MODE_NBE_CYCLES_VITESSE, ATTR_VISIBLE, 		bVisible);
			SetCtrlAttribute (iPanel, PANEL_MODE_NUM_CYCLE_VITESSE, 	ATTR_VISIBLE, 	bVisible);
			SetCtrlAttribute (iPanel, PANEL_MODE_NUM_CYCLE_PRESSION, ATTR_VISIBLE,		bVisible);

			SetCtrlAttribute (iPanel, PANEL_MODE_SAISIE_PRESSION_MANU, ATTR_VISIBLE, 	!bVisible);
			SetCtrlAttribute (iPanel, PANEL_MODE_SAISIE_VITESSE_MANU, ATTR_VISIBLE,		!bVisible);
			SetCtrlAttribute (iPanel, PANEL_MODE_SAISIE_DUREE_VIT_MANU, ATTR_VISIBLE, 	!bVisible);
			SetCtrlAttribute (iPanel, PANEL_MODE_SAISIE_DUREE_PRES_MAN, ATTR_VISIBLE, 	!bVisible);
			//SetCtrlAttribute (iPanel, PANEL_MODE_TABLE_SCRIPT, ATTR_VISIBLE, 	bVisible);
			SetCtrlAttribute (iPanel, PANEL_MODE_TABLE_SCRIPT, ATTR_VISIBLE, 	bDimmed); //modif by Carolina 02/2019

			SetCtrlAttribute (iPanel, PANEL_MODE_UNIT_PRESSION, ATTR_VISIBLE, 	1);
			SetCtrlAttribute (iPanel, PANEL_MODE_UNIT_VITESSE, ATTR_VISIBLE, 	1);
			SetCtrlAttribute (iPanel, PANEL_MODE_BUT_START_RUN, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_BUT_START_PRESSION, ATTR_VISIBLE, 	1);
			SetCtrlAttribute (iPanel, PANEL_MODE_BUTT_START_VITESSE, ATTR_VISIBLE, 	1);
			//SetCtrlAttribute (iPanel, PANEL_MODE_CHK_SHOW_ANUM, ATTR_VISIBLE, 	0);


			//InitAnum(FALSE);

			//modif carolina
			SetCtrlAttribute (iPanel, PANEL_MODE_INDICATOR_SET_LOG_DIR, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_INDICATOR_DATABASE, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_INDICATOR_CONFIG_ANUM, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_LED3, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_LED32, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_LED2, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_LED22, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_LED1, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_LED12, ATTR_VISIBLE, 	0);  
			SetCtrlAttribute (iPanel, PANEL_MODE_BUTTON_ANUM, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_BUTTON_DATABASE, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_BUTTON_SEE_GRAPH, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_PICTURE, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_MODE_CREATION, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_DECORATION, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_TEXTMSG1, ATTR_VISIBLE, 	0);
			//SetCtrlAttribute (iPanel, PANEL_MODE_SAVEFILES, ATTR_VISIBLE, 	0); //button
			SetCtrlAttribute (iPanel, PANEL_MODE_SAVEFILEBUTTON, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_BUTTON_SET_LOG_DIR, ATTR_VISIBLE, 	0);

			


			break;
			// Passage en mode automatique
		case iMODE_AUTOMATIQUE:
			bDimmed 	= 0;
			bVisible 	= 1;

			//------- Modif du 15/11/2010 par C.BERENGER ------ DEBUT ------------
			// Echelle des abscisses en mode automatique
			SetAxisScalingMode (GiPanel, PANEL_MODE_GRAPHE_VITESSE, VAL_BOTTOM_XAXIS, VAL_AUTOSCALE, 0.0, 100.0);
			SetAxisScalingMode (GiPanel, PANEL_MODE_GRAPHE_PRESSION, VAL_BOTTOM_XAXIS, VAL_AUTOSCALE, 0.0, 100.0);
			//------- Modif du 15/11/2010 par C.BERENGER ------ FIN ------------

			// Changement des libellés des boutons de démarrage des cycles
			iErr = SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VITESSE, ATTR_LABEL_TEXT, sBOUTON_DEP_VIT);
			// Traduction du bouton de Départ cycle pressions
			iErr = SetCtrlAttribute (GiPanel, PANEL_MODE_BUT_START_PRESSION, ATTR_LABEL_TEXT, sBOUTON_DEP_PRES);
			// Traduction du bouton de Départ cycle vitesses et pressions
			iErr = SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VIT_PRESS, ATTR_LABEL_TEXT, sBOUTON_DEP_VIT_PRES);

			// Déplacement du contrôle de choix d'unités
			iErr = SetCtrlAttribute (GiPanel, PANEL_MODE_UNIT_VITESSE, 	ATTR_TOP, 	iTOP_UNIT_VITESSE_AUTO);
			iErr = SetCtrlAttribute (GiPanel, PANEL_MODE_UNIT_PRESSION, ATTR_TOP, 	iTOP_UNIT_PRESSION_AUTO);


			// Force le tracé des courbes sur les graphiques
			//GstGrapheMajGrapheVit(GiPanel, PANEL_MODE_TABLE_VITESSES, PANEL_MODE_GRAPHE_VITESSE, 1, GstTabVitesse.dDuree, GstTabVitesse.dVit, GstTabVitesse.iNbElmts, 1, iCOUL_CONS_VITESSE, 1);
			//GstGrapheMajGraphePres(GiPanel, PANEL_MODE_TABLE_PRESSION, PANEL_MODE_GRAPHE_VITESSE, 1, GstTabPression.dDuree, GstTabPression.dPress, GstTabPression.iNbElmts, 1, iCOUL_CONS_PRESSION, 1);

			//Oprion cfg
			//SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_CONFIG_CHARGER, 	ATTR_DIMMED, 	bDimmed);  //Modif MaximePAGES 09/09/2020
			/* //Modif MaximePAGES 09/09/2020
			SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_CONFIG_ENREGISTRER, ATTR_DIMMED, 	bDimmed);
			SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_CONFIG_SEE_CURR_CYCLE, ATTR_DIMMED, 	bDimmed);
			SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_MODE_MANUEL, 		ATTR_DIMMED, 	!bDimmed);
			SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_MODE_AUTOMATIQUE, ATTR_DIMMED,	1);
			SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_MODE_RUN, ATTR_DIMMED,	0);
			*/
			
			/*SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_CONFIG_ITEM_LOAD_CONF, 	ATTR_DIMMED, 	1);			//Modif MaximePAGES 09/09/2020
			SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_CONFIG_ITEM_SAVE_CONF, 	ATTR_DIMMED, 	1);
			SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_CONFIG_ITEM_LOAD_ANUM_CFG, 	ATTR_DIMMED, 	1);
			SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_CONFIG_ITEM_LOAD_DATABASE, 	ATTR_DIMMED, 	1);
			SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_CONFIG_ITEM_SET_LOG_DIR, 	ATTR_DIMMED, 	1);   */

			SetCtrlAttribute (iPanel, PANEL_MODE_TABLE_VITESSES, ATTR_VISIBLE, 			bVisible);
			SetCtrlAttribute (iPanel, PANEL_MODE_TABLE_PRESSION, ATTR_VISIBLE, 			bVisible);

			SetCtrlAttribute (iPanel, PANEL_MODE_NBE_CYCLES_PRESSION, ATTR_VISIBLE, 	bVisible);
			SetCtrlAttribute (iPanel, PANEL_MODE_NBE_CYCLES_VITESSE, ATTR_VISIBLE, 		bVisible);
			SetCtrlAttribute (iPanel, PANEL_MODE_NUM_CYCLE_VITESSE, 	ATTR_VISIBLE, 	bVisible);
			SetCtrlAttribute (iPanel, PANEL_MODE_NUM_CYCLE_PRESSION, ATTR_VISIBLE,		bVisible);

			SetCtrlAttribute (iPanel, PANEL_MODE_SAISIE_PRESSION_MANU, ATTR_VISIBLE, 	!bVisible);
			SetCtrlAttribute (iPanel, PANEL_MODE_SAISIE_VITESSE_MANU, ATTR_VISIBLE,		!bVisible);
			SetCtrlAttribute (iPanel, PANEL_MODE_SAISIE_DUREE_VIT_MANU, ATTR_VISIBLE, 	!bVisible);
			SetCtrlAttribute (iPanel, PANEL_MODE_SAISIE_DUREE_PRES_MAN, ATTR_VISIBLE, 	!bVisible);
			//SetCtrlAttribute (iPanel, PANEL_MODE_TABLE_SCRIPT, ATTR_VISIBLE, 	!bVisible);
			SetCtrlAttribute (iPanel, PANEL_MODE_TABLE_SCRIPT, ATTR_VISIBLE, 	!bDimmed); //modif carolina 02/2019

			SetCtrlAttribute (iPanel, PANEL_MODE_UNIT_PRESSION, ATTR_VISIBLE, 	1);
			SetCtrlAttribute (iPanel, PANEL_MODE_UNIT_VITESSE, ATTR_VISIBLE, 	1);
			SetCtrlAttribute (iPanel, PANEL_MODE_BUT_START_RUN, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_BUT_START_PRESSION, ATTR_VISIBLE, 	1);
			SetCtrlAttribute (iPanel, PANEL_MODE_BUTT_START_VITESSE, ATTR_VISIBLE, 	1);
			//SetCtrlAttribute (iPanel, PANEL_MODE_CHK_SHOW_ANUM, ATTR_VISIBLE, 	0);

			//InitAnum(FALSE);

			//modif carolina
			SetCtrlAttribute (iPanel, PANEL_MODE_INDICATOR_SET_LOG_DIR, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_INDICATOR_DATABASE, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_INDICATOR_CONFIG_ANUM, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_LED3, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_LED32, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_LED2, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_LED22, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_LED1, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_LED12, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_BUTTON_ANUM, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_BUTTON_DATABASE, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_BUTTON_SEE_GRAPH, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_PICTURE, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_MODE_CREATION, ATTR_VISIBLE, 	0);
			SetCtrlAttribute(GiPanel, PANEL_MODE_MODE_ANALYSE, ATTR_VISIBLE,0);
			SetCtrlAttribute (iPanel, PANEL_MODE_DECORATION, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_TEXTMSG1, ATTR_VISIBLE, 	0);
			//SetCtrlAttribute (iPanel, PANEL_MODE_SAVEFILES, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_SAVEFILEBUTTON, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_BUTTON_SET_LOG_DIR, ATTR_VISIBLE, 	0);

		
			


			break;
		case iMODE_RUN:
			bDimmed 	= 0;
			bVisible 	= 1;

			//initialization of all components at the beginig
			//******************************************** these buttons don't exist in mode run
			//------- Modif du 15/11/2010 par C.BERENGER ------ DEBUT ------------
			// Echelle des abscisses en mode automatique
			SetAxisScalingMode (GiPanel, PANEL_MODE_GRAPHE_VITESSE, VAL_BOTTOM_XAXIS, VAL_AUTOSCALE, 0.0, 100.0);
			SetAxisScalingMode (GiPanel, PANEL_MODE_GRAPHE_PRESSION, VAL_BOTTOM_XAXIS, VAL_AUTOSCALE, 0.0, 100.0);
			//------- Modif du 15/11/2010 par C.BERENGER ------ FIN ------------

			SetAxisScalingMode (GiGraphPanel, PANELGRAPH_GRAPH, VAL_BOTTOM_XAXIS, VAL_AUTOSCALE, 0.0, 100.0);  //modif carolina

			//******************************************** these buttons don't exist in mode run
			// Changement des libellés des boutons de démarrage des cycles
			iErr = SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VITESSE, ATTR_LABEL_TEXT, sBOUTON_DEP_VIT);
			// Traduction du bouton de Départ cycle pressions
			iErr = SetCtrlAttribute (GiPanel, PANEL_MODE_BUT_START_PRESSION, ATTR_LABEL_TEXT, sBOUTON_DEP_PRES);
			// Traduction du bouton de Départ cycle vitesses et pressions
			iErr = SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VIT_PRESS, ATTR_LABEL_TEXT, sBOUTON_DEP_VIT_PRES);

			//******************************************** these buttons don't exist in mode run
			// Déplacement du contrôle de choix d'unités
			iErr = SetCtrlAttribute (GiPanel, PANEL_MODE_UNIT_VITESSE, 	ATTR_TOP, 	iTOP_UNIT_VITESSE_AUTO);
			iErr = SetCtrlAttribute (GiPanel, PANEL_MODE_UNIT_PRESSION, ATTR_TOP, 	iTOP_UNIT_PRESSION_AUTO);

			//******************************************** these buttons don't exist in mode run
			// Force le tracé des courbes sur les graphiques
			//GstGrapheMajGrapheVit(GiPanel, PANEL_MODE_TABLE_VITESSES, PANEL_MODE_GRAPHE_VITESSE, 1, GstTabVitesse.dDuree, GstTabVitesse.dVit, GstTabVitesse.iNbElmts, 1, iCOUL_CONS_VITESSE, 1);
			//GstGrapheMajGraphePres(GiPanel, PANEL_MODE_TABLE_PRESSION, PANEL_MODE_GRAPHE_VITESSE, 1, GstTabPression.dDuree, GstTabPression.dPress, GstTabPression.iNbElmts, 1, iCOUL_CONS_PRESSION, 1);

			//modif carolina
			//GstGrapheMajGrapheVit(GiGraphPanel, PANEL_MODE_TABLE_VITESSES, PANELGRAPH_GRAPH, 1, GstTabVitesse.dDuree, GstTabVitesse.dVit, GstTabVitesse.iNbElmts, 1, iCOUL_CONS_VITESSE, 1);
			//GstGrapheMajGraphePres(GiGraphPanel, PANEL_MODE_TABLE_PRESSION, PANELGRAPH_GRAPH, 1, GstTabPression.dDuree, GstTabPression.dPress, GstTabPression.iNbElmts, 1, iCOUL_CONS_PRESSION, 1);


			//Oprion cfg
			/*SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_CONFIG_CHARGER, 	ATTR_DIMMED, 	!bDimmed);   //Modif MaximePAGES 09/09/2020
			SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_CONFIG_ENREGISTRER, ATTR_DIMMED, 	!bDimmed);
			SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_CONFIG_SEE_CURR_CYCLE, ATTR_DIMMED, 	!bDimmed);
			SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_MODE_MANUEL, 		ATTR_DIMMED, 	!bDimmed);
			SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_MODE_AUTOMATIQUE, ATTR_DIMMED,	!bDimmed);
			SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_MODE_RUN, ATTR_DIMMED,	0);  */
			//SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_MODE_RUN, 		ATTR_BOLD , 1);
			//SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_MODE_RUN, 		ATTR_CHECKED, 1);

			//SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_CONFIG_ITEM_LOAD_CONF, 	ATTR_DIMMED, 	0);	   //Modif MaximePAGES 09/09/2020
			//SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_CONFIG_ITEM_SAVE_CONF, 	ATTR_DIMMED, 	0);   //Modif MaximePAGES 09/09/2020
			//SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_CONFIG_ITEM_LOAD_ANUM_CFG, 	ATTR_DIMMED, 	0);
			//SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_CONFIG_ITEM_LOAD_DATABASE, 	ATTR_DIMMED, 	0);
			//SetMenuBarAttribute (GetPanelMenuBar (iPanel), CONFIG_CONFIG_ITEM_SET_LOG_DIR, 	ATTR_DIMMED, 	0);

			SetCtrlAttribute (iPanel, PANEL_MODE_TABLE_VITESSES, ATTR_VISIBLE, 			!bVisible);
			SetCtrlAttribute (iPanel, PANEL_MODE_TABLE_PRESSION, ATTR_VISIBLE, 			!bVisible);

			SetCtrlAttribute (iPanel, PANEL_MODE_NBE_CYCLES_PRESSION, ATTR_VISIBLE, 	!bVisible);
			SetCtrlAttribute (iPanel, PANEL_MODE_NBE_CYCLES_VITESSE, ATTR_VISIBLE, 		!bVisible);
			SetCtrlAttribute (iPanel, PANEL_MODE_NUM_CYCLE_VITESSE, 	ATTR_VISIBLE, 	!bVisible);
			SetCtrlAttribute (iPanel, PANEL_MODE_NUM_CYCLE_PRESSION, ATTR_VISIBLE,		!bVisible);

			SetCtrlAttribute (iPanel, PANEL_MODE_SAISIE_PRESSION_MANU, ATTR_VISIBLE, 	!bVisible);
			SetCtrlAttribute (iPanel, PANEL_MODE_SAISIE_VITESSE_MANU, ATTR_VISIBLE,		!bVisible);
			SetCtrlAttribute (iPanel, PANEL_MODE_SAISIE_DUREE_VIT_MANU, ATTR_VISIBLE, 	!bVisible);
			SetCtrlAttribute (iPanel, PANEL_MODE_SAISIE_DUREE_PRES_MAN, ATTR_VISIBLE, 	!bVisible);
			//	SetCtrlAttribute (iPanel, PANEL_MODE_TABLE_SCRIPT, ATTR_VISIBLE, 	bVisible);

			//SetMenuBarAttribute (GiPopupMenuRunHandle, GST_RUN_MAIN_INSERT, 	ATTR_DIMMED, 	1);

			SetCtrlAttribute (iPanel, PANEL_MODE_UNIT_PRESSION, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_UNIT_VITESSE, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_BUT_START_RUN, ATTR_VISIBLE, 	1);
			SetCtrlAttribute (iPanel, PANEL_MODE_BUT_START_PRESSION, ATTR_VISIBLE, 	0);
			SetCtrlAttribute (iPanel, PANEL_MODE_BUTT_START_VITESSE, ATTR_VISIBLE, 	0);

			//******************************************** these buttons don't exist in mode run
			iErr = SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VIT_PRESS, ATTR_VISIBLE, 0);

			SetCtrlAttribute (GiPanel, PANEL_MODE_TABLE_SCRIPT,ATTR_WIDTH,300);
			SetCtrlAttribute (GiPanel, PANEL_MODE_TABLE_SCRIPT,ATTR_NUM_VISIBLE_COLUMNS,4);
			DeleteTableRows (GiPanel, PANEL_MODE_TABLE_SCRIPT, 1, -1);

			//SetCtrlAttribute (iPanel, PANEL_MODE_CHK_SHOW_ANUM, ATTR_VISIBLE, 	1);

			//InitAnum(TRUE);

			break;
	}



	// Mises à jour par rapport à l'unité de vitesse
	iErr = SetUnitVitesse(iPanel, PANEL_MODE_UNIT_VITESSE);
	// Mises à jour par rapport à l'unité de vitesse
	iErr = SetUnitPression(iPanel, PANEL_MODE_UNIT_PRESSION);

	//
	SetCtrlAttribute (iPanel, PANEL_MODE_BUTT_OUV_GACHE, ATTR_LABEL_TEXT, "Open Electric Lock");
}

// DEBUT ALGO
//******************************************************************************
//void CVICALLBACK ChgmentMode (int menuBar, int menuItem, void *callbackData,
//								int panel)
//******************************************************************************
//	- Paramètres CVI
//
//  - Changement de mode (automatique/manuel)
//
//  -  Aucun
//******************************************************************************
// FIN ALGO
void CVICALLBACK ChgmentMode (int menuBar, int menuItem, void *callbackData,
							  int panel)
{
	switch(menuItem)
	{
		/*case CONFIG_MODE_AUTOMATIQUE:   //Modif MaximePAGES 09/09/2020
			SetGiMode(iMODE_AUTOMATIQUE);
			DisplayMode(panel, iMODE_AUTOMATIQUE);
			break;
		case CONFIG_MODE_MANUEL:
			SetGiMode(iMODE_MANUEL);
			DisplayMode(panel, iMODE_MANUEL);
			break;
		case CONFIG_MODE_RUN:
			SetGiMode(iMODE_RUN);
			DisplayMode(panel, iMODE_RUN);
			char ProjectDirectory[MAX_PATHNAME_LEN];
			char *confpath;
			if(GetProjectDir(ProjectDirectory) == 0)
			{
				confpath = strcat(ProjectDirectory,"\\Config\\DefaultConfig.txt");
				int fSize = 0;
				if(FileExists(confpath,&fSize))  //FileExists(confpath,fSize)
				{
					LoadConfiguration(ProjectDirectory);
				}
			}
			else
			{
				MessagePopup ("Error", "Could not load default paths configuration!");
			}
			break;   */
	}
}

// DEBUT ALGO
//******************************************************************************
// void ModesDisplayPanel(int iMode)
//******************************************************************************
//	- int iMode : Mode automatique ou manuel
//
//  - Affichage de l'écran du mode automatique ou manuel
//
//  - 0 si OK, sinon code d'erreur
//******************************************************************************
// FIN ALGO
void ModesDisplayPanel(int iMode)
{
	double dDelay=0.0;
	int iStatus;
	//HANDLE threadHandle;
	stConfig *ptstConfig;

	// Configuration de l'affichage de l'écran en fonction du mode
	DisplayMode(GiPanel, iMode);
	// Affichage de l'écran
	DisplayPanel (GiPanel);
	DisplayCurrentCycle(GiPanel, GsPathConfigFile);
	SetPanelAttribute (GiPanel, ATTR_WINDOW_ZOOM, VAL_MAXIMIZE);  /// modif full screen

	SetGbQuitter(0);

	SetGiMode(iMode);
	SetGiEtat(iETAT_ATTENTE);

	//-----------------------------
	//---- Valeurs par défaut -----
	SetGbGacheActive(iGACHE_FERMEE);
	SetGiSelectionHandleVitesse(0);
	SetGiSelectionHandlePression(0);
	SetGiSelectionHandle(0);
	SetGiControlGrapheSelection(0);

	SetGiPanelStatusDisplayed(0);

	SetGiDisableMajGrapheVitesse(0);
	SetGiDisableMajGraphePression(0);

	SetGiStartCycleVitesse(0);
	SetGiStartCyclePression(0);
	SetGiStartVitPress(0);

	SetGbQuitter(0);

	SetGiHandleFileResult(-1);

	SetGbRazTimeVit(0);
	SetGbRazTimePress(0);

	SetGiErrUsb6008(0);
	SetGiErrVaria(0);
	SetGiIndexStepVitesse(1);
	SetGiIndexStepPression(1);

	SetGfVitesse(0.0);
	SetGdPression(0.0);
	SetGdPressionAProg(0.0);
	SetGdVitesseAProg(0.0);

	SetGiIndexCycleVitesse(1);
	SetGiTimeDepartEssaiSpeed(0);
	SetGiTimeDepartEssaiPress(0);

	SetGdDurEcoulEssaiSpeed(0.0);
	SetGdDurEcoulEssaiPress(0.0);
	SetGiIndexCyclePression(1);

	SetGbEndCycleVitesse(1);
	SetGbEndCyclePression(1);
	//----------------------------

	// Initialisation de la tache de gestion des cycles de vitesse
	//iStatus = CmtNewThreadPool (1, &GiThreadPoolHandleGestionVitesse);
	iStatus = CmtScheduleThreadPoolFunction (DEFAULT_THREAD_POOL_HANDLE , ThreadCycleVitesse, NULL, &GiThreadCycleVitesse);

	// Initialisation de la tache de gestion des cycles de pression
	//iStatus = CmtNewThreadPool (1, &GiThreadPoolHandleGestionPression);
	iStatus = CmtScheduleThreadPoolFunction (DEFAULT_THREAD_POOL_HANDLE, ThreadCyclePression, NULL, &GiThreadCyclePression);

	// Initialisation de la tache de gestion des sécurités
	//iStatus = CmtNewThreadPool (1, &GiThreadPoolHandleGestionSecurite);
	iStatus = CmtScheduleThreadPoolFunction (DEFAULT_THREAD_POOL_HANDLE, ThreadGestionSecurite, NULL, &GiThreadGestionSecurite);

	iStatus = CmtScheduleThreadPoolFunction (DEFAULT_THREAD_POOL_HANDLE, ThreadCycleRun, NULL, &GiThreadCycleRun);

	iStatus = CmtScheduleThreadPoolFunction (DEFAULT_THREAD_POOL_HANDLE, ThreadCycleAnum, NULL, &GiThreadCycleAnum);

	// Initialisation de la tache de gestion des acquisitions
	//iStatus = CmtNewThreadPool (1, &GiThreadPoolHandleAcquisitions);
	//iStatus = CmtScheduleThreadPoolFunction (GiThreadPoolHandleAcquisitions, ThreadAcquisitions, NULL, &GiThreadAcquisitions);

	ptstConfig = GetPointerToGptstConfig();
	dDelay = ((double)ptstConfig->iReadDelay) / 1000;
	GiThreadAcquisitions = NewAsyncTimer (dDelay, -1, 0, ThreadAcquisitions, NULL);
	ReleasePointerToGptstConfig();

	GbThreadPoolActive = 1;
}

// DEBUT ALGO
//******************************************************************************
// int ShowSelection(int iPanel, int iControlGrid, int iControlGraphe, int iColor)
//******************************************************************************
//	- int iPanel		: Handle du panel
//	  int iControlGrid	: Numéro de contrôle de la grille de données
//    int iControlGraphe: Numéro de contrôle du graphique
//	  int iColor		: Couleur de la sélection
//
//  - Montre sur le graphique la sélection en cours dans la table de données
//
//  - 0 si OK, sinon code d'erreur
//******************************************************************************
// FIN ALGO
int ShowSelection(int iPanel, int iControlGrid, int iControlGraphe, int iColor)
{
	Point pCell1;
	Point pCell2;
	double dValY;
	double dDuree;
	double dX1;
	double dX2;
	double dY1;
	double dY2;
	int iNumberOfRows;
	int iSelectionHandle;
	int iX;
	int iY;
	int iLeftButt;
	int iRightButt;
	int iLeft;
	int iTop;
	int i;
	int iStatus;
	int iStatus2;
	int iValue;


	// Détermination du nombre de lignes du contrôle (data grid)
	GetNumTableRows (iPanel, iControlGrid, &iNumberOfRows);

	// Lecture de la position (en pixels) du curseur de la souris sur le graphique
	GetGlobalMouseState (&iPanel, &iX, &iY, &iLeftButt, &iRightButt, NULL);

	// Lecture de la position du panel (coordonnées en haut à gauche)
	GetPanelAttribute (iPanel, ATTR_LEFT, &iLeft);
	GetPanelAttribute (iPanel, ATTR_TOP, &iTop);

	// Lecture de la valeur de la cellule par rapport à la position de la souris
	GetTableCellFromPoint (iPanel, iControlGrid, MakePoint (iX-iLeft, iY-iTop), &pCell1);

	// Si la veleur de la ligne dans la liste est correcte
	if ((pCell1.y <= iNumberOfRows)&&(pCell1.y > 0)&&(pCell1.x > 0)&&(pCell1.x < 3))
	{
		//---- Colonne des durées ------
		pCell2.x=2;
		dDuree = 0.0;
		// Ajout de toutes les durées
		for(i=1; i<=pCell1.y; i++)
		{
			pCell2.y = i;
			GetTableCellVal (iPanel, iControlGrid, pCell2, &dValY);
			dDuree += dValY;
		}

		//---- Colonne des valeurs (vitesse ou pression) ------
		pCell1.x = 1;
		// Lecture de la valeur en Y de la cellule sélectionnée
		GetTableCellVal (iPanel, iControlGrid, pCell1, &dValY);

		// Effacement de la dernière sélection sur le graphique
		iSelectionHandle = GetGiSelectionHandle();
		iStatus = GetPlotAttribute (iPanel, GetGiControlGrapheSelection(), iSelectionHandle, ATTR_PLOT_THICKNESS, &iValue);
		//modif carolina
		iStatus2 = GetPlotAttribute (GiGraphPanel, GetGiControlGrapheSelection(), iSelectionHandle, ATTR_PLOT_THICKNESS, &iValue);

		if(iStatus == 0)
			if (iSelectionHandle != 0)
				DeleteGraphPlot (iPanel, GetGiControlGrapheSelection(), iSelectionHandle, VAL_IMMEDIATE_DRAW);
		//modif carolina
		if(flag_seegraph == 1)
		{
			if(iStatus2 == 0)
				if (iSelectionHandle != 0)
					DeleteGraphPlot (GiGraphPanel, GetGiControlGrapheSelection(), iSelectionHandle, VAL_IMMEDIATE_DRAW);
		}

		dX2 = dDuree;
		dY2 = dValY;

		pCell1.x = 2;
		// Lecture de la valeur en Y de la cellule sélectionnée
		GetTableCellVal (iPanel, iControlGrid, pCell1, &dValY);


		dX1 = dDuree - dValY;

		// Colonne des données (vitesse ou pression)
		pCell1.x = 1;

		// Si la valeur de la ligne est correcte
		if (pCell1.y > 1)
		{
			pCell1.y -= 1;
			// Lecture de la valeur en Y de la cellule sélectionnée
			GetTableCellVal (iPanel, iControlGrid, pCell1, &dValY);
		}
		else
		{
			dX1 = 0.0;
			dValY = 0.0;//dY2; // Modif pour premier cycle
		}

		dY1 = dValY;


		// S'il y a plus d'un pixel de différence entre le point de départ et le point d'arrivée
		if ((abs(dX2 - dX1) >= 1.0) || (abs(dY2 - dY1)>= 1.0))
		{
			// Tracé d'une ligne
			SetGiSelectionHandle(PlotLine (GiPanel, iControlGraphe, dX1, dY1, dX2, dY2, iColor));
			//modif carolina
			SetGiSelectionHandle(PlotLine (GiGraphPanel, iControlGraphe, dX1, dY1, dX2, dY2, iColor));
			// Sauvegarde du handle du graphique de la dernière sélection
			SetGiControlGrapheSelection(iControlGraphe);

			// Taille du segment de droite = 3 pixels
			SetPlotAttribute (GiPanel, iControlGraphe, GetGiSelectionHandle(), ATTR_PLOT_THICKNESS, 3);
			SetPlotAttribute (GiPanel, iControlGraphe, GetGiSelectionHandle(), ATTR_TRACE_COLOR, iColor);
			//modif carolina
			SetPlotAttribute (GiGraphPanel, iControlGraphe, GetGiSelectionHandle(), ATTR_PLOT_THICKNESS, 3);
			SetPlotAttribute (GiGraphPanel, iControlGraphe, GetGiSelectionHandle(), ATTR_TRACE_COLOR, iColor);
		}
	}

	return 0;
}

// DEBUT ALGO
//******************************************************************************
// int CVICALLBACK GstTableRun(int panel, int control, int event,
//		                       void *callbackData, int eventData1, int eventData2)
//******************************************************************************
//  - Paramètres CVI
//
//  - Gestion du tableu Script Sequences
//
//  - 0 si OK, sinon code d'erreur
//
//Carolina :  Modification - netoyage des variables pas utilisé, int = 0
//******************************************************************************
// FIN ALGO

int CVICALLBACK  GstTableRun (int panel, int control, int event,
							  void *callbackData, int eventData1, int eventData2)  //GstTableRun = Script sequence
{
	Point cell;
	Rect  rect;
	BOOL bValid;

	//double dVal;
	int iError;
	int iX;
	int iY;
	int iTop;
	int iLeft;
	int iIdSelected;


	float TempoTotal = 0;


	char sProjectDir[MAX_PATHNAME_LEN];
	char ProjectDirectory[MAX_PATHNAME_LEN];
	char *confpath;
	char fileName[MAX_PATHNAME_LEN];
	int iRet=0;
		char sCellValue[200]="";
	char sCellValue2[200]="";

	int boucle=0;

	////////move
	int numeroLigne=0;
	

	switch (event)
	{

		case EVENT_RIGHT_CLICK:
			if((GetGiStartCycleRun() == 0))
			{

				
				SetCtrlAttribute (panel, PANEL_MODE_TABLE_SCRIPT, ATTR_ENABLE_POPUP_MENU, 0);

				// Lecture de la position du panel (coordonnées en haut à gauche)
				GetPanelAttribute (panel, ATTR_LEFT, &iLeft);
				GetPanelAttribute (panel, ATTR_TOP, &iTop);

				iError = GstTablesGetRowColFromMouse(panel, control, &cell.y, &cell.x, &iX, &iY, &bValid);

				// Détermination du nombre de lignes du contrôle (data grid)
				iError = GetNumTableRows (panel, control, &iNumberOfRows);

				// Si le nombre de lignes est supérieur à zéro
				iError = GstTablesGstZoneSelect(panel, control, cell.y, cell.x,
												&rect.height, &rect.width, &rect.left,
												&rect.top, &bValid);
				
					//	 printf("r = %d",rect.height); 
				//ReleasePointerToGptstConfig();
				iIdSelected = RunPopupMenu (GiPopupMenuRunHandle, GST_RUN_MAIN, panel, iY-iTop, iX-iLeft, 0, 0, 20, 20);
				switch(iIdSelected)
				{
					case GST_RUN_MAIN_ADD:
							addChoice = 1 ;
							InsertTestCasesIntoTables(iNumberOfRows + 1,panel);

						break;
					
					case GST_RUN_MAIN_INSERT:
						    //MARVYN 23/06/2021
							addChoice = 1 ;
							numRow = rect.top ;
						if ( numRow != 0)
						{
							couleurSelection(numRow,PANEL_MODE_TABLE_SCRIPT, panel,VAL_LT_GRAY );
							//****
							InsertTestCasesIntoTables(cell.y,panel);
							couleurSelection(numRow+1,PANEL_MODE_TABLE_SCRIPT, panel,VAL_WHITE );  
						}
						break;

					case GST_RUN_MAIN_DELETE:
						if(iNumberOfRows == 0)return 0;
						if (rect.top == 0)return 0;
						DeleteTableRows (panel, PANEL_MODE_TABLE_SCRIPT, rect.top, rect.height);
						break;

					case GST_RUN_MAIN_DELETEALL:
						if(iNumberOfRows == 0)return 0;
						DeleteTableRows (panel, PANEL_MODE_TABLE_SCRIPT, 1, -1);
						DeleteTableRows (panel, PANEL_MODE_SCRIPTS_DETAILS_2, 1, -1);
						SetCtrlVal(panel,PANEL_MODE_TOTALTIME, TempoTotal);
						SetCtrlVal(panel,PANEL_MODE_ENDTIMESEQUENCE, TempoTotal);
						ResetTextBox (GiPanel, PANEL_MODE_FAILEDSEQUENCE, "");
						break;

				/*	case GST_RUN_MAIN_SELECT:
						if(rect.height > 0 && cell.y != 0)
						{
							r = rect;
							// printf("r = %d",r.height);
							GetDataFromSelection(rect, PathsArray, RNoArray, &RowNb);
							 //printf("RowNb = %d",RowNb);
						//	SetMenuBarAttribute (GiPopupMenuRunHandle, GST_RUN_MAIN_INSERT, 	ATTR_DIMMED, 	0);
						}
						break;	  */

			
				//***************************** Marvyn 22/06/2021 *************************************
				// Move up and move down implementation in table script
					case GST_RUN_MAIN_MOVE_UP:

						numeroLigne = rect.top;

						if(numeroLigne<2)
						{
						}
						else
						{

							for(boucle=1; boucle < 4 ; boucle++)  
							{
								cell.x=boucle; // 1 
								cell.y =numeroLigne; // 3
								GetTableCellVal(panel,PANEL_MODE_TABLE_SCRIPT,cell, sCellValue); //collect info for the line we want to move
								cell.y =numeroLigne-1;  // move 1 row up 
								GetTableCellVal(panel,PANEL_MODE_TABLE_SCRIPT,cell, sCellValue2);
								SetTableCellVal(panel,PANEL_MODE_TABLE_SCRIPT,cell, sCellValue);
								cell.y =numeroLigne;	//move 1 row down after have exchanged the values
								SetTableCellVal(panel,PANEL_MODE_TABLE_SCRIPT,cell, sCellValue2);

							}

							//couleurLigne(numeroLigne, panel);
							//couleurLigne(numeroLigne-1, panel);

						}

						break;	
						
					case GST_RUN_MAIN_MOVE_DOWN:

						numeroLigne = rect.top;

						if(numeroLigne==iNumberOfRows)
						{
						}
						else
						{
							for(boucle=1; boucle < 4 ; boucle++)
							{
								cell.x=boucle;
								cell.y =numeroLigne;
								GetTableCellVal(panel,PANEL_MODE_TABLE_SCRIPT,cell, sCellValue);
								cell.y =numeroLigne+1;
								GetTableCellVal(panel,PANEL_MODE_TABLE_SCRIPT,cell, sCellValue2);
								SetTableCellVal(panel,PANEL_MODE_TABLE_SCRIPT,cell, sCellValue);
								cell.y =numeroLigne;
								SetTableCellVal(panel,PANEL_MODE_TABLE_SCRIPT,cell, sCellValue2);
							}

							/*couleurLigne(numeroLigne, panel);
							couleurLigne(numeroLigne+1, panel);*/

						}

						break;	
						

					case GST_RUN_MAIN_LOADSEQ:

						GetProjectDir(sProjectDir);
						strcpy(ProjectDirectory, sProjectDir);
						confpath = strcat(ProjectDirectory,"\\Config");
						iRet = FileSelectPopupEx (confpath, ".txt", ".txt", "", VAL_LOAD_BUTTON, 0, 1, fileName); 
						//iRet = FileSelectPopup (confpath, "*.","txt", "", VAL_LOAD_BUTTON, 0, 0, 1, 1, fileName);
						execonf = 0;
						
						strcpy(txtseqfile,fileName);
						if(iRet != 0) LoadConfiguration(fileName);

				}				 
			}
			break;

	}


	return 0;
}


void InsertTestCasesIntoTables(int RowPosition, int panel)
{
	Point cell;
	//Rect  rect;
	//BOOL bValid;

	//int EndBoucle = 0;
	//int boucle = 0;
	//char sCellValue[200]="";
	//char worksheetName[10]="TestCase";
	//char range[30];
	//int EndFromExcel=0;
	int numFiles;
	int iRet;
	char sFileName[500];
	//int bouclecopy=0;
	//int line = 1;
	//int TotalNumberLines = 0;
	//int TempoTotal=0;
	char** fileList;
	char sProjectDir[500];

	GetProjectDir(sProjectDir);

//	 iRet = MultiFileSelectPopup(sProjectDir, "*.""xlsx", "", "", 0, 0, 1, &numFiles, &fileList);
	iRet=MultiFileSelectPopupEx (sProjectDir, "*.xlsx","", "", 0, 0, &numFiles, &fileList); 
	
	if (iRet != VAL_NO_FILE_SELECTED)   //iRet != 0
	{
		if (fileList)
		{
			qsort ( fileList, numFiles, sizeof(char *), DataCheck);

			InsertTableRows (panel, PANEL_MODE_TABLE_SCRIPT, RowPosition, numFiles, VAL_CELL_STRING);

			// Using for loop to iterate through selected files
			for (int i=0; i < numFiles; i++)  //numFiles = number of TestScript selected
			{
				cell.y = RowPosition + i;
				cell.x = 3;
				SetTableCellVal (GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, fileList [i]);
				
				//we save the original script test path 
				cell.x = 5;
				SetTableCellVal (GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, fileList [i]);
				
				
				SplitPath(fileList [i],NULL,NULL,sFileName);
				cell.x = 1;
				SetTableCellVal (GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, sFileName);
				cell.x = 4;
				SetTableCellVal (GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, "1"); //1 par default

			}

		}
	}
	//MARVYN 23/06/2021
	if (addChoice == 0)
	{
	couleurSelection(numRow+numFiles,PANEL_MODE_TABLE_SCRIPT, panel,VAL_WHITE  )  ;
	}
}


int DataCheck(const void *a, const void *b)
{
	return strcmp(*(const char **)a, *(const char **)b);
}

void GetDataFromSelection(Rect rect, char PathsArray[500][500], char RNoArray[500][500], int *RowNb)
{
	*RowNb = rect.height;
	char sFileName[500];
	Point cell;

	for (int i=0; i < rect.height; i++)
	{
		cell.y = rect.top + i;
		cell.x = 3;
		GetTableCellVal (GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, PathsArray [i]);
		SplitPath(PathsArray [i],NULL,NULL,sFileName);

		cell.x = 4;
		GetTableCellVal (GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, RNoArray[i]);

	}
}

void InsertDataFromSelection(Point InsertCell, char PathsArray[500][500], char RNoArray[500][500], int RowNb)
{
	Point cell;
	char sFileName[500];
	//char sProjectDir[500];

	InsertTableRows (GiPanel, PANEL_MODE_TABLE_SCRIPT, InsertCell.y, RowNb, VAL_CELL_STRING);

	for (int i=0; i < RowNb; i++)
	{
		cell.y = InsertCell.y + i;
		cell.x = 3;
		SetTableCellVal (GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, PathsArray [i]);
		
		//we save the original script test path 
		cell.x = 5;
		SetTableCellVal (GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, PathsArray [i]);
		
		
		SplitPath(PathsArray [i],NULL,NULL,sFileName);

		cell.x = 1;
		SetTableCellVal (GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, sFileName);

		cell.x = 4;
		SetTableCellVal (GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, RNoArray[i]);

	}
}


// DEBUT ALGO
//******************************************************************************
//int CVICALLBACK GstTableVitesse (int panel, int control, int event,
//		void *callbackData, int eventData1, int eventData2)
//******************************************************************************
//	- Paramètres CVI
//
//  - Gestion des actions utilisateur sur la table de saisie des vitesses
//
//  - 0 si OK, sinon code d'erreur
//******************************************************************************
// FIN ALGO
int CVICALLBACK GstTableVitesse (int panel, int control, int event,
								 void *callbackData, int eventData1, int eventData2)
{
	Point cell;
	double dVal;
	int iPanelStatusDisplayed;
	int iAffMsg=0;

	iAffMsg=0;
	iPanelStatusDisplayed = 0;

	switch (event)
	{
		case EVENT_VAL_CHANGED:
			if ((GetGiStartCycleVitesse() == 0) && (GetGiStartCyclePression() == 0))
			{
				// Oblige l'utilisateur à sauver la configuration avant de lancer un essai
				strcpy(GsPathConfigFile, "");
				DisplayCurrentCycle(GiPanel, GsPathConfigFile);

				cell.x = eventData2;
				cell.y = eventData1;
				GetTableCellVal (panel, control, cell, &dVal);

				if (eventData2 == 1)
				{
					// Valeur de vitesse
					//CHECK_TAB_INDEX("1", eventData1-1, 0, 5000)
					GstTabVitesse.dVit[eventData1-1] 	= dVal;
				}
				else
				{
					// Durée
					//CHECK_TAB_INDEX("2", eventData1-1, 0, 5000)
					GstTabVitesse.dDuree[eventData1-1] 	= dVal;
				}

				/*if (!GetGiDisableMajGrapheVitesse())
				{
					// eventData1 = row of cell where event was generated; if 0, event affected multiple cells  
					// eventData2 = column of cell where event was generated; if 0, event affected multiple cells  
					GstGrapheMajGrapheVit(	panel, PANEL_MODE_TABLE_VITESSES, PANEL_MODE_GRAPHE_VITESSE, 1,
											GstTabVitesse.dDuree, GstTabVitesse.dVit, GstTabVitesse.iNbElmts, 1, iCOUL_CONS_VITESSE, 1);
					//modif carolina
					GstGrapheMajGrapheVit(	GiGraphPanel, PANEL_MODE_TABLE_VITESSES, PANELGRAPH_GRAPH, 1,
											GstTabVitesse.dDuree, GstTabVitesse.dVit, GstTabVitesse.iNbElmts, 1, iCOUL_CONS_VITESSE, 1);

				} */
			}
			break;
		case EVENT_RIGHT_CLICK:
			if((GetGiStartCycleVitesse() == 0) && (GetGiStartCyclePression() == 0))
			{
				GstDataGrid(panel, GiPopupMenuHandle, GiStatusPanel, GiPopupAdd,
							control, iTYPE_VITESSE, iCOUL_CONS_VITESSE, iCOUL_CONS_PRESSION, GetPointerToGptstConfig(),
							&GstTabVitesse, &GstTabPression,
							&iPanelStatusDisplayed, &iAffMsg, GsPathConfigFile);
				ReleasePointerToGptstConfig();

				SetGiAffMsg(iAffMsg);
				SetGiPanelStatusDisplayed(iPanelStatusDisplayed);
			}
			break;
		case EVENT_LEFT_CLICK:
			if ((GetGiStartCycleVitesse() == 0) && (GetGiStartCyclePression() == 0))
			{
				// Montre sur le graphique la sélection en cours dans la grille de données
				ShowSelection(panel, control, PANEL_MODE_GRAPHE_VITESSE, iCOUL_CONS_VITESSE);
			}
			break;
	}


	return 0;
}

// DEBUT ALGO
//******************************************************************************
//int CVICALLBACK GstTablePression (int panel, int control, int event,
//		void *callbackData, int eventData1, int eventData2)
//******************************************************************************
//	- Paramètres CVI
//
//  - Gestion des actions utilisateur sur la table de saisie des pressions
//
//  - 0 si OK, sinon code d'erreur
//******************************************************************************
// FIN ALGO
int CVICALLBACK GstTablePression (int panel, int control, int event,
								  void *callbackData, int eventData1, int eventData2)
{
	Point cell;
	double dVal=0.0;
	int iPanelStatusDisplayed=0;
	int iAffMsg=0;

	iAffMsg=0;
	iPanelStatusDisplayed=0;

	switch (event)
	{
		case EVENT_VAL_CHANGED:
			if((GetGiStartCycleVitesse() == 0) && (GetGiStartCyclePression() == 0))
			{
				// Oblige l'utilisateur à sauver la configuration avant de lancer un essai
				strcpy(GsPathConfigFile, "");
				DisplayCurrentCycle(GiPanel, GsPathConfigFile);

				cell.x = eventData2;
				cell.y = eventData1;
				GetTableCellVal (panel, control, cell, &dVal);

				if (eventData2 == 1)
				{
					// Valeur de pression
					//CHECK_TAB_INDEX("3", eventData1-1, 0, 5000)
					GstTabPression.dPress[eventData1-1] = dVal;
				}
				else
				{
					// Durée
					//CHECK_TAB_INDEX("4", eventData1-1, 0, 5000)
					GstTabPression.dDuree[eventData1-1] 	= dVal;
				}

				if (!GetGiDisableMajGraphePression())
				{
					/* eventData1 = row of cell where event was generated; if 0, event affected multiple cells  */
					/* eventData2 = column of cell where event was generated; if 0, event affected multiple cells  */
					//GstGrapheMajGraphePres(panel, PANEL_MODE_TABLE_PRESSION, PANEL_MODE_GRAPHE_VITESSE, 1, GstTabPression.dDuree, GstTabPression.dPress, GstTabPression.iNbElmts, 1, iCOUL_CONS_PRESSION, 1);

					if(flag_seegraph == 1)
					{
						//modif carolina
						//GstGrapheMajGraphePres(GiGraphPanel, PANEL_MODE_TABLE_PRESSION, PANELGRAPH_GRAPH, 1, GstTabPression.dDuree, GstTabPression.dPress, GstTabPression.iNbElmts, 1, iCOUL_CONS_PRESSION, 1);
					}
				}
			}
			break;
		case EVENT_RIGHT_CLICK:
			if((GetGiStartCycleVitesse() == 0) && (GetGiStartCyclePression() == 0))
			{
				GstDataGrid(panel, GiPopupMenuHandle, GiStatusPanel, GiPopupAdd,
							control, iTYPE_PRESSION, iCOUL_CONS_VITESSE, iCOUL_CONS_PRESSION, GetPointerToGptstConfig(),
							&GstTabVitesse, &GstTabPression,
							GetPointerToGiPanelStatusDisplayed(), &iAffMsg, GsPathConfigFile);
				ReleasePointerToGptstConfig();
				ReleasePointerToGiPanelStatusDisplayed();
				SetGiAffMsg(iAffMsg);
			}
			break;
		case EVENT_LEFT_CLICK:
			if((GetGiStartCycleVitesse() == 0) && (GetGiStartCyclePression() == 0))
			{
				// Montre sur le graphique la sélection en cours dans la grille de données
				ShowSelection(panel, control, PANEL_MODE_GRAPHE_PRESSION, iCOUL_CONS_PRESSION);
			}
			break;
	}


	return 0;
}

void LoadTest(char *sFileName,int panel)
{
#define iTAILLE_MAX_ERR_MSG	5000
#define iBKG_ERROR_COLOR	0x00FFDDDD
	char sTpsEcoule[50];
	char sErrorMsg[iTAILLE_MAX_ERR_MSG];
	char sComments[iMAX_CHAR_COMMENTS+2];
	//char sProjectDir[MAX_PATHNAME_LEN];

	int iNbCyclesVit;
	int iNbCyclesPres;
	//int iRet;
	int iErr;
	int i;
	int iUnitVitesse;
	int iUnitPression;
	//int iPanelStatusDisplayed;
	int iAffMsg;
	//int iHeures;
	//int iMinutes;
	//int iSecondes;
	Point cell;
	Rect rect;
	BOOL bSaveOneVit;
	BOOL bSaveOnePres;

	// On inactive l'écran
	SetPanelAttribute (panel, ATTR_DIMMED, 1);

	// Recopie du chemin complet vers le fichier de configuration
	strcpy(GsPathConfigFile, sFileName);	// BenchTest.csv
	DisplayCurrentCycle(GiPanel, GsPathConfigFile);

	iErr = GstFilesLoadConfig (	GiPanel, PANEL_MODE_TABLE_VITESSES, PANEL_MODE_TABLE_PRESSION, PANEL_MODE_MESSAGES_MODE,
								GetPointerToGptstConfig(), &GstTabVitesse, &GstTabPression,
								&iNbCyclesVit, &iNbCyclesPres,
								&iUnitVitesse, &iUnitPression,
								sComments, &bSaveOneVit, &bSaveOnePres, sFileName,
								sErrorMsg, iTAILLE_MAX_ERR_MSG - 1);
	ReleasePointerToGptstConfig();
	if(iErr != 0)
	{
		strcpy(GsPathConfigFile, "");
	}

	if(iErr >= 0)
	{
		SetGiUnitPression(iUnitPression);
		SetGiUnitVitesse(iUnitVitesse);
	}

	DisplayCurrentCycle(GiPanel, GsPathConfigFile);

	// S'il y a eu une erreur (le fichier n'a pas pu être chargé)
	if(iErr < 0)
	{
		// Mise à jour de la mémoire à partir des tableaux de données
		GstTablesMajTableau(GiPanel, PANEL_MODE_TABLE_VITESSES, iTYPE_VITESSE,
							&GstTabVitesse, &GstTabPression, sErrorMsg);
		GstTablesMajTableau(GiPanel, PANEL_MODE_TABLE_PRESSION, iTYPE_PRESSION,
							&GstTabVitesse, &GstTabPression, sErrorMsg);

		//modif carolina

		GstTablesMajTableau(GiGraphPanel, PANEL_MODE_TABLE_VITESSES, iTYPE_VITESSE,
							&GstTabVitesse, &GstTabPression, sErrorMsg);
		GstTablesMajTableau(GiGraphPanel, PANEL_MODE_TABLE_PRESSION, iTYPE_PRESSION,
							&GstTabVitesse, &GstTabPression, sErrorMsg);
	}

	if(iErr < 0)
	{
		AffStatusPanel(GiPanel, GiStatusPanel, iLEVEL_MSG,sMSG_ERR_LOADING_FILE,
					   sErrorMsg, GetPointerToGiPanelStatusDisplayed(), &iAffMsg);
		ReleasePointerToGiPanelStatusDisplayed();

		SetGiAffMsg(iAffMsg);

		// On active l'écran
		SetPanelAttribute (panel, ATTR_DIMMED, 0);
		return;
	}

	if(iErr==2)
	{
		AffStatusPanel(GiPanel, GiStatusPanel, iLEVEL_MSG,sMSG_ERR_READING_FILE,
					   sErrorMsg, GetPointerToGiPanelStatusDisplayed(), &iAffMsg);
		ReleasePointerToGiPanelStatusDisplayed();

		SetGiAffMsg(iAffMsg);
	}

	SetGiDisableMajGrapheVitesse(1);
	SetGiDisableMajGraphePression(1);

	// Supression des données du tableau des vitesses
	DeleteTableRows (panel, PANEL_MODE_TABLE_VITESSES, 1, -1);
	// Supression des données du tableau des pressions
	DeleteTableRows (panel, PANEL_MODE_TABLE_PRESSION, 1, -1);

	// Fiwe les limites des tableaux de vitesse et de pression
	iErr = GstTablesSetLim(panel, PANEL_MODE_TABLE_VITESSES, PANEL_MODE_TABLE_PRESSION, GetGiUnitVitesse(),
						   GetGiUnitPression(), GetPointerToGptstConfig());
	ReleasePointerToGptstConfig();

	//----- Modification du 15/11/2010 par C.BERENGER ---- DEBUT ----------
	// Fixe les limites des saisies manuelles de vitesse et pression
	iErr = SetLimSaisiesMan(GiPanel, PANEL_MODE_SAISIE_VITESSE_MANU, PANEL_MODE_SAISIE_PRESSION_MANU, PANEL_MODE_SAISIE_DUREE_VIT_MANU,
							PANEL_MODE_SAISIE_DUREE_PRES_MAN, GetGiUnitVitesse(), GetGiUnitPression(), GetPointerToGptstConfig());
	ReleasePointerToGptstConfig();
	//----- Modification du 15/11/2010 par C.BERENGER ---- FIN ----------


	// Insertion du nombre de lignes souhaité pour le tableau des vitesses
	InsertTableRows (panel, PANEL_MODE_TABLE_VITESSES, 1, GstTabVitesse.iNbElmts, VAL_CELL_NUMERIC);
	// Insertion du nombre de lignes souhaité pour le tableau des pressions
	InsertTableRows (panel, PANEL_MODE_TABLE_PRESSION, 1, GstTabPression.iNbElmts, VAL_CELL_NUMERIC);


	//----- Mise à jour de la table des vitesses -----
	rect.height = GstTabVitesse.iNbElmts;
	rect.left 	= 1;
	rect.top 	= 1;
	rect.width 	= 1;
	SetTableCellRangeVals (GiPanel, PANEL_MODE_TABLE_VITESSES, rect, GstTabVitesse.dVit, VAL_COLUMN_MAJOR);

	rect.height = GstTabVitesse.iNbElmts;
	rect.left 	= 2;
	rect.top 	= 1;
	rect.width 	= 1;
	SetTableCellRangeVals (GiPanel, PANEL_MODE_TABLE_VITESSES, rect, GstTabVitesse.dDuree, VAL_COLUMN_MAJOR);

	for(i=0; i<GstTabVitesse.iNbElmts; i++)
	{
		//CHECK_TAB_INDEX("5", i, 0, 5000)
		if(GstTabVitesse.bWarningData[i])
		{
			// Coloration de la cellule en rouge
			cell.x = 1;
			cell.y = i+1;
			SetTableCellAttribute (GiPanel, PANEL_MODE_TABLE_VITESSES, cell, ATTR_TEXT_BGCOLOR, iBKG_ERROR_COLOR);
		}

		if(GstTabVitesse.bWarningDuree[i])
		{
			// Coloration de la cellule en rouge
			cell.x = 2;
			cell.y = i+1;
			SetTableCellAttribute (GiPanel, PANEL_MODE_TABLE_VITESSES, cell, ATTR_TEXT_BGCOLOR, iBKG_ERROR_COLOR);
		}
	}

	for(i=0; i<GstTabPression.iNbElmts; i++)
	{
		//CHECK_TAB_INDEX("6", i, 0, 5000)
		if(GstTabPression.bWarningData[i])
		{
			// Coloration de la cellule en rouge
			cell.x = 1;
			cell.y = i+1;
			SetTableCellAttribute (GiPanel, PANEL_MODE_TABLE_PRESSION, cell, ATTR_TEXT_BGCOLOR, iBKG_ERROR_COLOR);
		}

		if(GstTabPression.bWarningDuree[i])
		{
			// Coloration de la cellule en rouge
			cell.x = 2;
			cell.y = i+1;
			SetTableCellAttribute (GiPanel, PANEL_MODE_TABLE_PRESSION, cell, ATTR_TEXT_BGCOLOR, iBKG_ERROR_COLOR);
		}
	}

	// Mise à jour du graphique des vitesses
	//GstGrapheMajGrapheVit(panel, PANEL_MODE_TABLE_VITESSES, PANEL_MODE_GRAPHE_VITESSE, 1, GstTabVitesse.dDuree, GstTabVitesse.dVit, GstTabVitesse.iNbElmts, 1, iCOUL_CONS_VITESSE, 1);

	//modif carolina
	//GstGrapheMajGrapheVit(GiGraphPanel, PANEL_MODE_TABLE_VITESSES, PANELGRAPH_GRAPH, 1, GstTabVitesse.dDuree, GstTabVitesse.dVit, GstTabVitesse.iNbElmts, 1, iCOUL_CONS_VITESSE, 1);


	//----- Mise à jour de la table des pressions ----
	rect.height = GstTabPression.iNbElmts;
	rect.left 	= 1;
	rect.top 	= 1;
	rect.width 	= 1;
	SetTableCellRangeVals (GiPanel, PANEL_MODE_TABLE_PRESSION, rect, GstTabPression.dPress, VAL_COLUMN_MAJOR);

	rect.height = GstTabPression.iNbElmts;
	rect.left 	= 2;
	rect.top 	= 1;
	rect.width 	= 1;
	SetTableCellRangeVals (GiPanel, PANEL_MODE_TABLE_PRESSION, rect, GstTabPression.dDuree, VAL_COLUMN_MAJOR);

	// Mise à jour du graphique des pressions
	//GstGrapheMajGraphePres(panel, PANEL_MODE_TABLE_PRESSION, PANEL_MODE_GRAPHE_VITESSE, 1, GstTabPression.dDuree, GstTabPression.dPress, GstTabPression.iNbElmts, 1, iCOUL_CONS_PRESSION, 1);

	//modif carolina
	//GstGrapheMajGraphePres(GiGraphPanel, PANEL_MODE_TABLE_PRESSION, PANELGRAPH_GRAPH, 1, GstTabPression.dDuree, GstTabPression.dPress, GstTabPression.iNbElmts, 1, iCOUL_CONS_PRESSION, 1);

	//------------------------------

	// Mise à jour du nombre de cycle de vitesse
	SetCtrlVal (GiPanel, PANEL_MODE_NBE_CYCLES_VITESSE, iNbCyclesVit);

	// Mise à jour du nombre de cycle de pression
	SetCtrlVal (GiPanel, PANEL_MODE_NBE_CYCLES_PRESSION, iNbCyclesPres);

	SetGiDisableMajGrapheVitesse(0);
	SetGiDisableMajGraphePression(0);

	// Mise à jour par rapport à l'unité de vitesse
	iErr = SetUnitVitesse(panel, PANEL_MODE_UNIT_VITESSE);
	// Mise à jour par rapport à l'unité de pression
	iErr = SetUnitPression(panel, PANEL_MODE_UNIT_PRESSION);

	// Mise à jour du cycle en cours
	SetCtrlVal (panel, PANEL_MODE_NUM_CYCLE_VITESSE, 	0);
	SetCtrlVal (panel, PANEL_MODE_NUM_CYCLE_PRESSION, 	0);

	// Mise à jour du temps d'essai écoulé
	sprintf(sTpsEcoule, sFORMAT_DUREE_ECOULEE, 0, 0, 0);
	SetCtrlVal (panel, PANEL_MODE_AFFICH_TPS_VIT, 		sTpsEcoule);
	SetCtrlVal (panel, PANEL_MODE_AFFICH_TPS_PRESS, 	sTpsEcoule);

	// On active l'écran
	SetPanelAttribute (panel, ATTR_DIMMED, 0);
}

// DEBUT ALGO
//******************************************************************************
//void CVICALLBACK Configuration (int menuBar, int menuItem, void *callbackData,
//		int panel)
//******************************************************************************
//	- Paramètres CVI
//
//  - Gestion des fichiers de données (vitesse et pression)
//
//  - 0 si OK, sinon code d'erreur
//******************************************************************************
// FIN ALGO
void CVICALLBACK Configuration (int menuBar, int menuItem, void *callbackData,
								int panel)
{
#define iTAILLE_MAX_ERR_MSG	5000
#define iBKG_ERROR_COLOR	0x00FFDDDD
	//char sTpsEcoule[50];

	//char sComments[iMAX_CHAR_COMMENTS+2];
	char sProjectDir[MAX_PATHNAME_LEN];
	

	


	//int i;
	//int iUnitVitesse;
	//int iUnitPression;
	//int iPanelStatusDisplayed;

	//int iHeures;
	//int iMinutes;
	//int iSecondes;

	//Point cell;
	//Rect rect;
	//BOOL bSaveOneVit;
	//BOOL bSaveOnePres;

	//char **fileList = NULL, **ppc = NULL;
	//int numFiles;




	

	// Formation du chemin vers le répertoire des configurations
	GetProjectDir(sProjectDir);
	//strcat(sProjectDir, "\\"sREP_CONFIGURATIONS);

	switch (menuItem)
	{
	/*	case CONFIG_CONFIG_CHARGER:		//Modif MaximePAGES 09/09/2020

			iRet = FileSelectPopup (sProjectDir, "*."sEXT_FICHIERS, "", "", VAL_SELECT_BUTTON, 0, 1, 1, 0, sFileName);

			if (iRet != VAL_NO_FILE_SELECTED)
			{
				LoadTest(sFileName,panel);
			}
			break;	   */
	/*	case CONFIG_CONFIG_ENREGISTRER:		 //Modif MaximePAGES 09/09/2020
			// Modification du 01/02/2012 par C. BERENGER ---- DEBUT -----
			// La restriction au répertoire courant a été supprimée à la demande du client


			iRet = FileSelectPopup (sProjectDir, "*."sEXT_FICHIERS, "", "", VAL_SAVE_BUTTON, 0, 1, 1, 0, sFileName);
			// Modification du 01/02/2012 par C. BERENGER ---- FIN -----

			if (iRet != VAL_NO_FILE_SELECTED)
			{
				// On inactive l'écran
				SetPanelAttribute (panel, ATTR_DIMMED, 1);

				GetCtrlVal (GiPanel, PANEL_MODE_NBE_CYCLES_VITESSE, 	&iNbCyclesVit);
				GetCtrlVal (GiPanel, PANEL_MODE_NBE_CYCLES_PRESSION, &iNbCyclesPres);

				iErr =  GstFilesSaveConfig (panel, &GstTabVitesse, &GstTabPression,
											iNbCyclesVit, iNbCyclesPres,
											GetGiUnitVitesse(), GetGiUnitPression(),
											bSAVE_ONE_VIT, bSAVE_ONE_PRES,
											bSAVE_VIT, bSAVE_PRES, sFileName,
											sErrorMsg, iTAILLE_MAX_ERR_MSG - 1);

				if (iErr == 0)
				{
					// Recopie du chemin complet vers le fichier de configuration
					strcpy(GsPathConfigFile, sFileName);
					DisplayCurrentCycle(GiPanel, GsPathConfigFile);
				}
				else
				{
					AffStatusPanel(GiPanel, GiStatusPanel, iLEVEL_MSG,sMSG_ERR_SAV_CYCLES, sErrorMsg, GetPointerToGiPanelStatusDisplayed(), &iAffMsg);
					ReleasePointerToGiPanelStatusDisplayed();

					SetGiAffMsg(iAffMsg);
				}

				// On active l'écran
				SetPanelAttribute (panel, ATTR_DIMMED, 0);
			}
			break;  */
	/*	case CONFIG_CONFIG_ITEM_SAVE_CONF:  //Modif MaximePAGES 09/09/2020

			strcpy(ProjectDirectory, sProjectDir);
			confpath = strcat(ProjectDirectory,"\\Config");

			SaveConfiguration(iRet, confpath);

			break;		 */
	/*	case CONFIG_CONFIG_ITEM_LOAD_CONF:  //Modif MaximePAGES 09/09/2020

			strcpy(ProjectDirectory, sProjectDir);
			confpath = strcat(ProjectDirectory,"\\Config");

			iRet = FileSelectPopup (confpath, "*.","txt", "", VAL_LOAD_BUTTON, 0, 0, 1, 1, fileName);
			if(iRet != 0) LoadConfiguration(fileName);

			break;  */
			/*		case CONFIG_CONFIG_ITEM_LOAD_ANUM_CFG:

						iRet = FileSelectPopup (sProjectDir, "*."".cfg", "", "", VAL_SELECT_BUTTON, 0, 1, 1, 0, GsAnumCfgFile);

						iAnumLoad=1;   // Condition pour le boutton Add script

						//Récupération du nom de la config ANum

						int curseur_chaine;
						int compteur_chaine=0;
						int fin_chaine=0;
						char config_name[MAX_PATHNAME_LEN]="";

						//     C:\Users\uid42961\Desktop\Test_11.4_WU TestBench Auto Tool (LC) - Amélioration\Automation_ANum_Config.cfg
								for(curseur_chaine=strlen(GsAnumCfgFile); fin_chaine==0 ;curseur_chaine--)
									{

									if( GsAnumCfgFile[curseur_chaine]=='\\' )
									   {
										fin_chaine=1;
										for( compteur_chaine=0; (curseur_chaine+compteur_chaine) < strlen(GsAnumCfgFile) ;compteur_chaine++)
											{
											 config_name[compteur_chaine]=GsAnumCfgFile[curseur_chaine+compteur_chaine+1];
											}
										}
									}
									//printf("\n Name: %s", config_name );

									SetCtrlVal (panel, PANEL_MODE_LED_CONFIG_ANUM, 1);
									SetCtrlVal (panel, PANEL_MODE_INDICATOR_CONFIG_ANUM, config_name);

						//printf("\n%s",GsAnumCfgFile);
						//MessagePopup ("Warning", "ANum not initialized");

					//	if(!GbAnumInit)
					//	{
					//		MessagePopup ("Warning", "ANum not initialized");
					//		return;
					//	}
						//iRet = FileSelectPopup (sProjectDir, "*."".cfg", "", "", VAL_SELECT_BUTTON, 0, 1, 1, 0, sFileName);

					//	if (iRet != VAL_NO_FILE_SELECTED)
					//	{
					//
					//		//LoadANumConfiguration(sFileName);
					//	}

					//	break;
					case CONFIG_CONFIG_ITEM_LOAD_DATABASE:
						//printf("%s",GsCheckCfgFile);
						iRet = FileSelectPopup (sProjectDir, "*."".xlsx", "", "", VAL_SELECT_BUTTON, 0, 1, 1, 0, GsCheckCfgFile);
						//printf("%s",GsCheckCfgFile);

						iDataBaseLoad=1;   // Condition pour le boutton Add script

						if (iRet != VAL_NO_FILE_SELECTED)
						{

						}
						//Récupération du nom de la database

						int curseur_chaine2=0;
						int compteur_chaine2=0;
						int fin_chaine2=0;
						char database_name[MAX_PATHNAME_LEN]="";

						//     C:\Users\uid42961\Desktop\Test_11.4_WU TestBench Auto Tool (LC) - Amélioration\Automation_ANum_Config.cfg
								for(curseur_chaine2=strlen(GsCheckCfgFile); fin_chaine2==0 ;curseur_chaine2--)
									{

									if( GsCheckCfgFile[curseur_chaine2]=='\\' )
									   {
										fin_chaine2=1;
										for( compteur_chaine2=0; (curseur_chaine2+compteur_chaine2) < strlen(GsCheckCfgFile) ;compteur_chaine2++)
											{
											 database_name[compteur_chaine2]=GsCheckCfgFile[curseur_chaine2+compteur_chaine2+1];
											}
										}
									}
									//printf("\n Name: %s", database_name );

									SetCtrlVal (panel, PANEL_MODE_LED_DATABASE, 1);
									SetCtrlVal (panel, PANEL_MODE_INDICATOR_DATABASE, database_name);

					break;
					case CONFIG_CONFIG_ITEM_SET_LOG_DIR:

						iSetLogDirLoad = 1;
						iRet = DirSelectPopup (sProjectDir, "Select Log Directory", 1, 1, GsLogPath);

						if (iRet != VAL_NO_DIRECTORY_SELECTED)
						{

						}
						break;	   */

	}

	//return 0;
}

void LoadANumConfiguration(char sFileName[MAX_PATHNAME_LEN])
{
	struct    TAPILFRF sAPILFRF;
	unsigned char cAPI;
	char aCommand[128];
	TAPI myFUnc = 0;

	myFUnc= GapiANum;
	strcpy(aCommand, "LOAD");
	strcpy(sAPILFRF.StringIn1, sFileName);
	cAPI=myFUnc(aCommand, (void*)&sAPILFRF);

	if (cAPI!=0)
	{
		ModesAddMessageZM("Failed to load ANUM cfg!");
		GbAnumInit=0;
		return;
	}

	strcpy(aCommand, "LFPWR");
	sAPILFRF.ByteIn1=100;
	cAPI=myFUnc(aCommand, (void*)&sAPILFRF);

	ModesAddMessageZM(sFileName);
}


void SaveConfiguration(int iRet, char *sProjectDir)
{

	if(!CheckPrecondition())
		return;

	iRet = FileSelectPopupEx (sProjectDir, "*.txt", "", "", VAL_SAVE_BUTTON, 0, 1, GsSaveConfigPathsFile); 
	//iRet = FileSelectPopup (sProjectDir, "*."".txt", "", "", VAL_SAVE_BUTTON, 0, 1, 1, 0, GsSaveConfigPathsFile);
	if(iRet == 0) return;

	int f = OpenFile (GsSaveConfigPathsFile, VAL_WRITE_ONLY, VAL_TRUNCATE, VAL_ASCII);

	int a =  WriteLine (f,GsAnumCfgFile,-1);
	int b =  WriteLine (f,GsCheckCfgFile,-1);
	int c =  WriteLine (f,GsLogPathInit,-1);

	if(a != -1 && b != -1 && c != -1)
	{
		MessagePopup("Message","Paths Configuration File Save Succeded!");
	}
	else
	{
		Beep();
		MessagePopup("Error","Cannot save Paths Configuration File!");
		CloseFile(f);
		return;
	}
	int rowNo;
	GetNumTableRows(GiPanel, PANEL_MODE_TABLE_SCRIPT,&rowNo);

	if(WriteLine (f,"",-1) == -1)
	{
		MessagePopup("Error","Error Encountered while saving Paths Configuration File!");
		CloseFile(f);
		return;
	}
	if(WriteLine (f,"Tests:",-1) == -1)
	{
		MessagePopup("Error","Error Encountered while saving Paths Configuration File!");
		CloseFile(f);
		return;
	}

	int i;
	Point cell;
	for(i = 1; i<= rowNo; i++)
	{
		cell.x = 5;
		cell.y = i;
		char auxString[MAX_PATHNAME_LEN];
		char auxStrNum[MAX_PATHNAME_LEN];

		GetTableCellVal (GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, auxString);
		cell.x = 4;
		GetTableCellVal (GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, auxStrNum);
		//int auxNum = 0;

		if(WriteLine (f,auxString,-1) == -1)
		{

			MessagePopup("Error","Error Encountered while saving Paths Configuration File!");
			CloseFile(f);
			return;
		}

		if(WriteLine (f,auxStrNum,-1) == -1)
		{
			MessagePopup("Error","Error Encountered while saving Paths Configuration File!");
			CloseFile(f);
			return;
		}
	}
	
	//Mofid MaximePAGES 11/08/2020 
	firstCurrentSeqSaved = 1;
	
	CloseFile(f);
}

void LoadConfiguration(char *sFile)
{
	char sFileName[MAX_PATHNAME_LEN]="";
	strcpy(sFileName,sFile);
	
	int f = OpenFile (sFileName, VAL_READ_ONLY, VAL_TRUNCATE, VAL_ASCII);

	if(ReadLine (f,GsAnumCfgFile,-1) < 0)
	{
		MessagePopup("Error","Wrong ANum Path or Paths Configuration File not compliant!");
		return ;
	}
	if(ReadLine (f,GsCheckCfgFile,-1) < 0)
	{
		MessagePopup("Error","Wrong \".xlsx\" Configuration File Path or Paths Configuration File not compliant!");
		return ;
	}
	if(ReadLine (f,GsLogPath,-1) < 0)
	{
		MessagePopup("Error","Wrong Log Storage Directory Path or Paths Configuration File not compliant!");
		return ;
	}

	//LoadANumConfiguration(GsAnumCfgFile);

	char TestName[MAX_PATHNAME_LEN];
	char TestNumber[10];
	ReadLine (f,TestName,-1);
	ReadLine (f,TestName,-1);

	DeleteTableRows (GiPanel, PANEL_MODE_TABLE_SCRIPT, 1, -1);

	int RowPosition = 1;
	int nbTests = 0;

	while(ReadLine (f,TestName,-1) != -2 && ReadLine (f,TestNumber,-1) != -2)
	{
		int fSize = 0;

		Point cell;

		if(FileExists(TestName,&fSize))
		{
			InsertTableRows (GiPanel, PANEL_MODE_TABLE_SCRIPT, RowPosition, 1, VAL_CELL_STRING);

			cell.y = RowPosition;
			cell.x = 3;
			SetTableCellVal (GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, TestName);
			
			//we save the original script test path
			cell.x = 5;
			SetTableCellVal (GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, TestName);
			
			SplitPath(TestName,NULL,NULL,sFileName);
			cell.x = 1;
			SetTableCellVal (GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, sFileName);
			cell.x = 4;
			SetTableCellVal (GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, TestNumber);

			RowPosition ++;
		}
		else
		{
			nbTests++;
		}
	}
	if(nbTests != 0)
	{
		char *auxString;
		char aux[MAX_PATHNAME_LEN];

		Fmt (aux, "%d", nbTests);
		auxString = strcat(aux," Test-scripts paths not valid!");

	}
}
// DEBUT ALGO
//******************************************************************************
//int CVICALLBACK QuitterModeAuto (int panel, int control, int event,
//		void *callbackData, int eventData1, int eventData2)
//******************************************************************************
//	- Paramètres CVI
//
//  - Quitte l'écran d'essai
//
//  - 0 si OK, sinon code d'erreur
//******************************************************************************
// FIN ALGO
void CVICALLBACK Quitter (int menuBar, int menuItem, void *callbackData,
						  int panel)
{
	int iStatus;

	SetGbQuitter(1);
	SetGiEtat(iETAT_ATTENTE_AFFICH_IHM);

	// Inactivation de la gâche
	SetGiErrUsb6008(Usb6008CommandeOuvertureGache (iUSB6008_CMDE_OUVERTURE_GACHE_INACTIVE));

	// S'il n'y a pas d'erreur
	if(GetGiErrUsb6008() == 0)
	{
		SetGbGacheActive(iGACHE_FERMEE);
		SetCtrlAttribute (panel, PANEL_MODE_BUTT_OUV_GACHE, ATTR_LABEL_TEXT, "Open Electric Lock");
	}

	// Arrêt de la ventilation moteur
	SetGiErrUsb6008(Usb6008CommandeVentilationMoteur (iUSB6008_CMDE_VENTILATION_MOTEUR_INACTIFS));

	if(GbThreadPoolActive)
	{
		// Attente d'arrêt du Thread de gestion des cycles
		iStatus = CmtWaitForThreadPoolFunctionCompletion (DEFAULT_THREAD_POOL_HANDLE , GiThreadCycleVitesse,OPT_TP_PROCESS_EVENTS_WHILE_WAITING);
		iStatus = CmtReleaseThreadPoolFunctionID (DEFAULT_THREAD_POOL_HANDLE , GiThreadCycleVitesse);

		iStatus = CmtWaitForThreadPoolFunctionCompletion (DEFAULT_THREAD_POOL_HANDLE , GiThreadCyclePression, OPT_TP_PROCESS_EVENTS_WHILE_WAITING);
		iStatus = CmtReleaseThreadPoolFunctionID (DEFAULT_THREAD_POOL_HANDLE , GiThreadCyclePression);

		iStatus = CmtWaitForThreadPoolFunctionCompletion (DEFAULT_THREAD_POOL_HANDLE, GiThreadGestionSecurite, OPT_TP_PROCESS_EVENTS_WHILE_WAITING);
		iStatus = CmtReleaseThreadPoolFunctionID (DEFAULT_THREAD_POOL_HANDLE, GiThreadGestionSecurite);

		iStatus = CmtWaitForThreadPoolFunctionCompletion (DEFAULT_THREAD_POOL_HANDLE , GiThreadCycleRun,OPT_TP_PROCESS_EVENTS_WHILE_WAITING);
		iStatus = CmtReleaseThreadPoolFunctionID (DEFAULT_THREAD_POOL_HANDLE , GiThreadCycleRun);

		iStatus = CmtWaitForThreadPoolFunctionCompletion (DEFAULT_THREAD_POOL_HANDLE , GiThreadCycleAnum,OPT_TP_PROCESS_EVENTS_WHILE_WAITING);
		iStatus = CmtReleaseThreadPoolFunctionID (DEFAULT_THREAD_POOL_HANDLE , GiThreadCycleAnum);
		//iStatus = CmtWaitForThreadPoolFunctionCompletion (GiThreadPoolHandleAcquisitions, GiThreadAcquisitions, OPT_TP_PROCESS_EVENTS_WHILE_WAITING);
		//iStatus = CmtReleaseThreadPoolFunctionID (GiThreadPoolHandleAcquisitions, GiThreadAcquisitions);
		//iStatus = CmtDiscardThreadPool (GiThreadPoolHandleAcquisitions);

		DiscardAsyncTimer (-1);
		GiThreadAcquisitions = 0;

		GbThreadPoolActive = 0;
		//----------------------------
		//----------------------------
	}

	//InitAnum(FALSE);

	// Effacement de la zone de messages
	ZoneMessageSaturee(GiPanel, PANEL_MODE_MESSAGES_MODE, sNOM_LIBRAIRIE, 1);
	// Programmation du retour Ihm
	PostDeferredCall ((DeferredCallbackPtr) GFonctionRetourIhm, &GiEvtRetourIhm);

	//Close Excel Database
	ExcelRpt_WorkbookClose (workbookHandledata, 0);
	ExcelRpt_ApplicationQuit (applicationHandleProject);
	if(iDataBaseLoad == 1)
		CA_DiscardObjHandle(applicationHandleProject);
	applicationHandleProject   = 0;

	killPIDprocess(databasePID);  //MODIF MaximePAGES 23/06/2020 - E002 - We kill the database Excel process.
	killAllProcExcel(myListProcExcelPID);//kill tous les processus excel  
	free(myParameterTab);




	//CA_DiscardObjHandle(applicationLabel);
	//CA_DiscardObjHandle(applicationHandle4);


	iAnumLoad=0;
	strcpy(GsAnumCfgFile, "");
	SetCtrlVal(panel,PANEL_MODE_INDICATOR_CONFIG_ANUM,"");
	SetCtrlVal (panel, PANEL_MODE_LED1, 0);
	SetCtrlVal (panel, PANEL_MODE_LED12, 0x00000000); 

	iDataBaseLoad=0;
	strcpy(GsCheckCfgFile, "");
	SetCtrlVal(panel,PANEL_MODE_INDICATOR_DATABASE,"");
	SetCtrlVal (panel, PANEL_MODE_LED2, 0);
	SetCtrlVal (panel, PANEL_MODE_LED22, 0x00000000);
	ResetTextBox (GiPanel, PANEL_MODE_FAILEDSEQUENCE, "");

	iSetLogDirLoad = 0;
	strcpy(GsLogPath, "");
	SetCtrlVal(panel,PANEL_MODE_INDICATOR_SET_LOG_DIR,"");
	SetCtrlVal (panel, PANEL_MODE_LED32, 0x00000000);
	SetCtrlVal (panel, PANEL_MODE_LED3, 0);

	// Fermeture du panel
	HidePanel (GiPanel);
}


//*************************ADDED BY MARVYN 09/06/2021*****************************************************
//int CVICALLBACK QuitterModeAuto (int panel, int control, int event,
//		void *callbackData, int eventData1, int eventData2)
//******************************************************************************
//	- Paramètres CVI
//
//  - Quitte l'écran d'essai
//
//  - 0 si OK, sinon code d'erreur
//******************************************************************************
int CVICALLBACK redCross (int panel, int control, int event,void *callbackData, int eventData1, int eventData2)
{
		int iStatus;
		int iYes; 
	switch (event)
	{
		case EVENT_COMMIT:
	
		

	iYes = ConfirmPopup ("Warning", sMSG_WARNING_QUIT);
	if(iYes == 1)
	{

		SetGbQuitter(1);
		SetGiEtat(iETAT_ATTENTE_AFFICH_IHM);

		// Inactivation de la gâche
		SetGiErrUsb6008(Usb6008CommandeOuvertureGache (iUSB6008_CMDE_OUVERTURE_GACHE_INACTIVE));

		// S'il n'y a pas d'erreur
		if(GetGiErrUsb6008() == 0)
		{
			SetGbGacheActive(iGACHE_FERMEE);
			SetCtrlAttribute (panel, PANEL_MODE_BUTT_OUV_GACHE, ATTR_LABEL_TEXT, "Open Electric Lock");
		}

		// Arrêt de la ventilation moteur
		SetGiErrUsb6008(Usb6008CommandeVentilationMoteur (iUSB6008_CMDE_VENTILATION_MOTEUR_INACTIFS));

		if(GbThreadPoolActive)
		{
			// Attente d'arrêt du Thread de gestion des cycles
			iStatus = CmtWaitForThreadPoolFunctionCompletion (DEFAULT_THREAD_POOL_HANDLE , GiThreadCycleVitesse,OPT_TP_PROCESS_EVENTS_WHILE_WAITING);
			iStatus = CmtReleaseThreadPoolFunctionID (DEFAULT_THREAD_POOL_HANDLE , GiThreadCycleVitesse);

			iStatus = CmtWaitForThreadPoolFunctionCompletion (DEFAULT_THREAD_POOL_HANDLE , GiThreadCyclePression, OPT_TP_PROCESS_EVENTS_WHILE_WAITING);
			iStatus = CmtReleaseThreadPoolFunctionID (DEFAULT_THREAD_POOL_HANDLE , GiThreadCyclePression);

			iStatus = CmtWaitForThreadPoolFunctionCompletion (DEFAULT_THREAD_POOL_HANDLE, GiThreadGestionSecurite, OPT_TP_PROCESS_EVENTS_WHILE_WAITING);
			iStatus = CmtReleaseThreadPoolFunctionID (DEFAULT_THREAD_POOL_HANDLE, GiThreadGestionSecurite);

			iStatus = CmtWaitForThreadPoolFunctionCompletion (DEFAULT_THREAD_POOL_HANDLE , GiThreadCycleRun,OPT_TP_PROCESS_EVENTS_WHILE_WAITING);
			iStatus = CmtReleaseThreadPoolFunctionID (DEFAULT_THREAD_POOL_HANDLE , GiThreadCycleRun);

			iStatus = CmtWaitForThreadPoolFunctionCompletion (DEFAULT_THREAD_POOL_HANDLE , GiThreadCycleAnum,OPT_TP_PROCESS_EVENTS_WHILE_WAITING);
			iStatus = CmtReleaseThreadPoolFunctionID (DEFAULT_THREAD_POOL_HANDLE , GiThreadCycleAnum);
			//iStatus = CmtWaitForThreadPoolFunctionCompletion (GiThreadPoolHandleAcquisitions, GiThreadAcquisitions, OPT_TP_PROCESS_EVENTS_WHILE_WAITING);
			//iStatus = CmtReleaseThreadPoolFunctionID (GiThreadPoolHandleAcquisitions, GiThreadAcquisitions);
			//iStatus = CmtDiscardThreadPool (GiThreadPoolHandleAcquisitions);

			DiscardAsyncTimer (-1);
			GiThreadAcquisitions = 0;

			GbThreadPoolActive = 0;
			//----------------------------
			//----------------------------
		}

		//InitAnum(FALSE);

		// Effacement de la zone de messages
		ZoneMessageSaturee(GiPanel, PANEL_MODE_MESSAGES_MODE, sNOM_LIBRAIRIE, 1);
		// Programmation du retour Ihm
		PostDeferredCall ((DeferredCallbackPtr) GFonctionRetourIhm, &GiEvtRetourIhm);

		//Close Excel Database
		ExcelRpt_WorkbookClose (workbookHandledata, 0);
		ExcelRpt_ApplicationQuit (applicationHandleProject);
		if(iDataBaseLoad == 1)
			CA_DiscardObjHandle(applicationHandleProject);
		applicationHandleProject   = 0;

		killPIDprocess(databasePID);  //MODIF MaximePAGES 23/06/2020 - E002 - We kill the database Excel process.
		free(myParameterTab);




		//CA_DiscardObjHandle(applicationLabel);
		//CA_DiscardObjHandle(applicationHandle4);


		iAnumLoad=0;
		strcpy(GsAnumCfgFile, "");
		SetCtrlVal(panel,PANEL_MODE_INDICATOR_CONFIG_ANUM,"");
		SetCtrlVal (panel, PANEL_MODE_LED1, 0);
		SetCtrlVal (panel, PANEL_MODE_LED12, 0x00000000); 

		iDataBaseLoad=0;
		strcpy(GsCheckCfgFile, "");
		SetCtrlVal(panel,PANEL_MODE_INDICATOR_DATABASE,"");
		SetCtrlVal (panel, PANEL_MODE_LED2, 0);
		SetCtrlVal (panel, PANEL_MODE_LED22, 0x00000000);
		ResetTextBox (GiPanel, PANEL_MODE_FAILEDSEQUENCE, "");

		iSetLogDirLoad = 0;
		strcpy(GsLogPath, "");
		SetCtrlVal(panel,PANEL_MODE_INDICATOR_SET_LOG_DIR,"");
		SetCtrlVal (panel, PANEL_MODE_LED32, 0x00000000);
		SetCtrlVal (panel, PANEL_MODE_LED3, 0);

		// Fermeture du panel
		HidePanel (GiPanel);

				break;
	}
	}
	return 0;
}



// DEBUT ALGO
//******************************************************************************
// int SetUnitVitesse(int iPanel, int iControl)
//******************************************************************************
//	- int iPanel	: Handle du panel
//	  int iControl	: Handle du contrôle
//
//  - Changement de l'unité de vitesse
//
//  - 0 si OK, sinon code d'erreur
//******************************************************************************
// FIN ALGO
int SetUnitVitesse(int iPanel, int iControl)
{
	char sUnit[100];
	char sAffich[100];
	int iErr;
	int iNbRows;
	Rect rect;

	// Recopie des données du tableau de données vers la data grid
	GetNumTableRows (iPanel, PANEL_MODE_TABLE_VITESSES, &iNbRows);
	rect.height = iNbRows;
	rect.left 	= 1;
	rect.top 	= 1;
	rect.width 	= 1;

	switch(GetGiUnitVitesse())
	{
		case iUNIT_KM_H:
			(sUnit, sUNIT_VIT_KM_H);
			strcpy(sAffich, sAFFICH_VITESSE_KM_H);
			break;
		case iUNIT_G:
			strcpy(sUnit, sUNIT_VIT_G);
			strcpy(sAffich, sAFFICH_VITESSE_G);
			break;
		case iUNIT_TRS_MIN:
			strcpy(sUnit, sUNIT_VIT_TRS_MIN);
			strcpy(sAffich, sAFFICH_VITESSE_TRS_MIN);
			break;
	}

	// Mise à jour des libellés
	iErr = SetTableColumnAttribute (iPanel, PANEL_MODE_TABLE_VITESSES, 	1,ATTR_LABEL_TEXT, 	sUnit);
	iErr = SetCtrlAttribute (iPanel, PANEL_MODE_AFFICH_VITESSE, 		ATTR_LABEL_TEXT, 	sAffich);
	iErr = SetCtrlAttribute (iPanel, PANEL_MODE_GRAPHE_VITESSE, 		ATTR_YNAME, 		sAffich);
	iErr = SetCtrlAttribute (iPanel, PANEL_MODE_SAISIE_VITESSE_MANU, 	ATTR_LABEL_TEXT, 	sAffich); //mode manual
	iErr = SetCtrlAttribute (GiStatusPanel, PANEL_STAT_SPEED, 			ATTR_LABEL_TEXT, 	sAffich);  //panel status
	iErr = SetCtrlVal 		(iPanel, PANEL_MODE_UNIT_VITESSE, 			sUnit);

//************************* modif carolina
	//test show_accel in the panel graph - mode run
	if(flag_seegraph == 1)
	{
		iErr = SetCtrlAttribute (GiGraphPanel, PANELGRAPH_SHOW_ACCEL, 		ATTR_LABEL_TEXT, 	sAffich);
		iErr = SetCtrlAttribute (GiGraphPanel, PANELGRAPH_GRAPH, 		ATTR_YNAME, 		sAffich);
		//	iErr = SetCtrlVal 		(GiGraphPanel, PANEL_MODE_UNIT_VITESSE, 			sUnit);
	}


	// Recopie des données du tableau de données vers la data grid
	SetTableCellRangeVals (iPanel, PANEL_MODE_TABLE_VITESSES, rect, GstTabVitesse.dVit, VAL_COLUMN_MAJOR);

	/*if ((GetGiMode() == iMODE_AUTOMATIQUE)||(GetGiMode() == iMODE_RUN))
	{
		// Mise à jour du graphique (uniquement en mode automatique)
		GstGrapheMajGrapheVit(iPanel, PANEL_MODE_TABLE_VITESSES, PANEL_MODE_GRAPHE_VITESSE, 1, GstTabVitesse.dDuree, GstTabVitesse.dVit, GstTabVitesse.iNbElmts, 1, iCOUL_CONS_VITESSE, 1);
		//modif carolina
		GstGrapheMajGrapheVit(GiGraphPanel, PANEL_MODE_TABLE_VITESSES, PANELGRAPH_GRAPH, 1, GstTabVitesse.dDuree, GstTabVitesse.dVit, GstTabVitesse.iNbElmts, 1, iCOUL_CONS_VITESSE, 1);

	} */

	return 0;//OK
}

// DEBUT ALGO
//******************************************************************************
// int SetUnitPression(int iPanel, int iControl)
//******************************************************************************
//	- int iPanel	: Handle du panel
//	  int iControl	: Handle du contrôle
//
//  - Changment de l'unité de pression
//
//  - 0 si OK, sinon code d'erreur
//******************************************************************************
// FIN ALGO
int SetUnitPression(int iPanel, int iControl)
{
	char sUnit[100];
	char sAffich[100];
	int iErr;
	int iNbRows;
	Rect rect;

	// Recopie des données du tableau de données vers la data grid
	GetNumTableRows (iPanel, PANEL_MODE_TABLE_PRESSION, &iNbRows); //Returns the number of rows
	rect.height = iNbRows;
	rect.left 	= 1;
	rect.top 	= 1;
	rect.width 	= 1;

	switch(GetGiUnitPression())
	{
		case iUNIT_KPA:
			strcpy(sUnit, sUNIT_PRES_KPA);
			strcpy(sAffich, sAFFICH_PRES_KPA);
			break;
		case iUNIT_BAR:
			strcpy(sUnit, sUNIT_PRES_BAR);
			strcpy(sAffich, sAFFICH_PRES_BARS);
			break;
		case iUNIT_MBAR:
			strcpy(sUnit, sUNIT_PRES_MBAR);
			strcpy(sAffich, sAFFICH_PRES_MBAR);
			break;
		case iUNIT_PSI:
			strcpy(sUnit, sUNIT_PRES_PSI);
			strcpy(sAffich, sAFFICH_PRES_PSI);
			break;
	}

	iErr = SetTableColumnAttribute (iPanel, PANEL_MODE_TABLE_PRESSION, 1,	ATTR_LABEL_TEXT, 	sUnit);
	iErr = SetCtrlAttribute (iPanel, PANEL_MODE_AFFICH_PRESS, 				ATTR_LABEL_TEXT, 	sAffich);
	iErr = SetCtrlAttribute (iPanel, PANEL_MODE_GRAPHE_PRESSION, 			ATTR_YNAME, 		sAffich);
	iErr = SetCtrlAttribute (iPanel, PANEL_MODE_SAISIE_PRESSION_MANU, 		ATTR_LABEL_TEXT, 	sAffich);
	iErr = SetCtrlAttribute (GiStatusPanel, PANEL_STAT_PRESSURE, 			ATTR_LABEL_TEXT, 	sAffich);
	iErr = SetCtrlVal (iPanel, PANEL_MODE_UNIT_PRESSION, 					sUnit);

	// Recopie des données du tableau de données vers la data grid
	SetTableCellRangeVals (iPanel, PANEL_MODE_TABLE_PRESSION, rect, GstTabPression.dPress, VAL_COLUMN_MAJOR);

	//************************* modif carolina
	//test show_pres in the panel graph
	if(flag_seegraph == 1)
	{
		iErr = SetCtrlAttribute (GiGraphPanel, PANELGRAPH_SHOW_PRES, 		ATTR_LABEL_TEXT, 	sAffich);
		iErr = SetCtrlAttribute (GiGraphPanel, PANELGRAPH_GRAPH, 			ATTR_YNAME, 		sAffich);
		//iErr = SetCtrlVal 		(GiGraphPanel, PANEL_MODE_UNIT_PRESSION, 			sUnit);
	}

	return 0; // OK
}

// DEBUT ALGO
//******************************************************************************
// int CreateRepResultIfNeeded(void)
//******************************************************************************
//	- Aucun
//
//  - Création du répertoire résultats si nécessaire
//
//  - 0 si OK, sinon code d'erreur
//******************************************************************************
// FIN ALGO
int CreateRepResultIfNeeded(void)
{
	char sDateHeure[100];
	int iErr;

	if (GsPathResult[0] == 0x00)
	{
		iErr = GetProjectDir(GsPathResult);
		GstFilesGetStringDateHeure(sDateHeure);

		strcat(GsPathResult, "\\");
		strcat(GsPathResult, sREP_RESULTATS"\\");
		strcat(GsPathResult, sDateHeure);

		// Création du répertoire des résultats
		iErr= MakeDir (GsPathResult);
	}

	return 0; //OK
}

// DEBUT ALGO -- Mode AUTOMATIQUE
//******************************************************************************
//int CVICALLBACK BoutonVitesse (int panel, int control, int event,
//		void *callbackData, int eventData1, int eventData2)
//******************************************************************************
//	- Paramètres CVI
//
//  - Gestion du bouton de départ/arrêt cycle de vitesse
//
//  - 0 si OK, sinon code d'erreur
//******************************************************************************
// FIN ALGO
int CVICALLBACK BoutonVitesse (int panel, int control, int event,
							   void *callbackData, int eventData1, int eventData2)
{
	char sTitle[200];
	int iNumberOfRows;
	int iErr;
	int iYes=0;

	switch (event)
	{
		case EVENT_COMMIT:
			// Si le cycle de vitesse est en cours
			if (GetGiStartCycleVitesse()	== 1)
				iYes = ConfirmPopup ("Warning", "Are you sure you want to stop Test?");

			if (GetGiStartCycleVitesse() == 1)
			{
				if(iYes)
				{
					// Arrêt de la tâche d'acquisition
					//SetAsyncTimerAttribute (GiThreadAcquisitions, ASYNC_ATTR_ENABLED, 0);

					iErr = GstIhmCycles(panel,0,  0, GetGiStartCyclePression());
					SetGiStartCycleVitesse(0);
					// Remise à zéro du handle de fichier résultats
					//SetGiHandleFileResult(-1);
				}
			}
			else
			{
				if (GetGiMode() == iMODE_AUTOMATIQUE)
				{
					// Détermination du nombre de lignes du contrôle (data grid)
					iErr = GetNumTableRows (panel, PANEL_MODE_TABLE_VITESSES, &iNumberOfRows);

					if(strlen(GsPathConfigFile) == 0)
					{
						iErr = -1;
						CHECK_ERROR(iErr, "Error", sMSG_SAVE_DATA_ON_FILE)
						return 0;
					}
				}
				else
					iNumberOfRows = 2;

				if (iNumberOfRows >= 2)
				{
					// Commande de la ventilation forcée avant de lancer un cycle moteur
					SetGiErrUsb6008(Usb6008CommandeVentilationMoteur (iUSB6008_CMDE_VENTILATION_MOTEUR_ACTIFS));

					// Initialisations
					SetGdDurEcoulEssaiSpeed(0.0);
					SetGiIndexStepVitesse(1);
					SetGiIndexCycleVitesse(1);

					// Lancement du cycle de vitesse
					SetGiStartCycleVitesse(1);

					// Lancement de la tâche d'acquisition
					SetAsyncTimerAttribute (GiThreadAcquisitions, ASYNC_ATTR_ENABLED, 1);
				}
				else
				{
					// On ne peut pas lancer le cycle de vitesse
					ModesAddMessageZM(sMSG_CANT_START_SPEED_CYCLE);
				}

				iErr = GstIhmCycles(panel,GetGiStartVitPress(),  GetGiStartCycleVitesse(), GetGiStartCyclePression());
			}

			break;
	}
	return 0;
}

void ExecuteAnumCmd(int cmd, char *strVal, int numVal, int param, char *ID)
{
	unsigned char cAPI;
	char aCommand[128];
	TAPI      myFUnc=0;                  // Pointeur on API function (described in API.h)
	struct    TAPILFRF sAPILFRF;
	char sAnumPath[MAX_PATHNAME_LEN];
//	myFUnc= GetGiAnumApi();
	myFUnc=GapiANum;
	//printf("numval = %d and param = %d\n",numVal,param);   
	if(myFUnc!=NULL)
	{
		switch(cmd)
		{
			case 1:
			{
				strcpy(aCommand, "LFPWR");
				sAPILFRF.ByteIn1=(unsigned char)numVal;
				sprintf (sAnumPath, "%s\\%s\\%s", GsPathApplication,"Config", sANUM_CFG);
				strcpy(sAPILFRF.StringIn1, sAnumPath);  // \Config\Anum.cfg
				cAPI=myFUnc(aCommand, (void*)&sAPILFRF);
				
				break;
			}
			case 2:
			{
				strcpy(aCommand, "SENDF");
				sAPILFRF.ByteIn1=(unsigned char)numVal;
				sAPILFRF.ByteIn2=(unsigned char)param;
				strcpy(sAPILFRF.StringIn1, strVal);
				cAPI=myFUnc(aCommand, (void*)&sAPILFRF);
				
				
				if (cAPI!=0)
				{
				//printf("error\n");
				return;
				}
				break;
			}
			case 3:
			{
				strcpy(aCommand, "STOPF");
				cAPI=myFUnc(aCommand, (void*)&sAPILFRF);
				break;
			}
			case 4:
			{
				strcpy(aCommand, "RUN");
				//cAPI=myFUnc("SHOW", (void*)&sAPILFRF);
				cAPI=myFUnc(aCommand, (void*)&sAPILFRF);

				if (cAPI!=0)
					cAPI=myFUnc(aCommand, (void*)&sAPILFRF);
				break;
			}
			case 5:
			{
				strcpy(aCommand, "STOP");
				cAPI=myFUnc(aCommand, (void*)&sAPILFRF);
				break;
			}
			case 6:
			{
				//sprintf (sAnumPath, "%s\\%s\\%s", GsPathApplication,"Config", sANUM_CFG);
				strcpy(aCommand, "LOAD");
				strcpy(sAPILFRF.StringIn1, GsAnumCfgFile);
				cAPI=myFUnc(aCommand, (void*)&sAPILFRF);

				strcpy(aCommand, "LFPWR");
				sAPILFRF.ByteIn1=100;
				cAPI=myFUnc(aCommand, (void*)&sAPILFRF);
				break;
			}
			case 7:
			{
			//	if(numVal==1)
			//	{
					strcpy(aCommand, "SHOW");
					
					sAPILFRF.ByteIn1=(unsigned char)0;
					sAPILFRF.ByteIn2=(unsigned char)0;
					sAPILFRF.ByteIn3=(unsigned char)0;
					sAPILFRF.ByteIn4=(unsigned char)0;
					strcpy(sAPILFRF.StringIn1, "");
					strcpy(sAPILFRF.StringIn2, "");
					strcpy(sAPILFRF.StringIn3, "");
					strcpy(sAPILFRF.StringIn4, "");
					
					sAPILFRF.ByteOut1=(unsigned char)0;
					sAPILFRF.ByteOut2=(unsigned char)0;
					sAPILFRF.ByteOut3=(unsigned char)0;
					sAPILFRF.ByteOut4=(unsigned char)0;
					strcpy(sAPILFRF.StringOut1, "");
					strcpy(sAPILFRF.StringOut2, "");
					strcpy(sAPILFRF.StringOut3, "");
					strcpy(sAPILFRF.StringOut4, "");
					
					cAPI=myFUnc(aCommand, (void*)&sAPILFRF);
					
				if (cAPI!=0)
				{
				//printf("error\n");
				return;
				} 
			//	}
			/*	if(numVal==0)
				{
					strcpy(aCommand, "HIDE");
					cAPI=myFUnc(aCommand, (void*)&sAPILFRF);
				}   */
				break;
			}
			case 8:
			{
	
				strcpy(aCommand, "SENDF");
				sAPILFRF.ByteIn1=(unsigned char)numVal;
				sAPILFRF.ByteIn2=(unsigned char)param;
				strcpy(sAPILFRF.StringIn1, strVal);
				strcpy(sAPILFRF.StringIn2, ID);
				
				//printf("strlen = %d\n",strlen(ID));
				
				cAPI=myFUnc(aCommand, (void*)&sAPILFRF);
				
				if (cAPI!=0)
				{
			//	//printf("error\n");
				return;
				} 
					
				
				break;
			}
			case 9:
			{
				strcpy(aCommand, "SENDF");
				sAPILFRF.ByteIn3=(unsigned char)numVal;
				sAPILFRF.ByteIn4=(unsigned char)param;
				strcpy(sAPILFRF.StringIn1, strVal);
				cAPI=myFUnc(aCommand, (void*)&sAPILFRF);
				
				
				break;
			}
		}
	}

}

BOOL CheckPrecondition()
{
	ssize_t fSize;
	if(!FileExists(GsAnumCfgFile,&fSize))
	{
		Beep();
		MessagePopup("Warning","Please select the ANum .cfg file!");
		return FALSE;
	}
	if(!FileExists(GsCheckCfgFile,&fSize))
	{
		Beep();
		MessagePopup("Warning","Please select the .xlsx tests config file!");
		return FALSE;
	}
	//Modif MARVYN 12/07/2021
	if(!FileExists(GsLogPath,&fSize))
	{
		Beep();
		MessagePopup("Warning","Please select the Log folder!");
		return FALSE;
	}

	//anum cfg
	//tb ready for running- check status of the buttons
	return TRUE;
}

int CVICALLBACK OnCheckShowAnum (int panel, int control, int event,
								 void *callbackData, int eventData1, int eventData2)
{
	//char sTpsEcoule[50];
	//int iErr;
	//int rowNo=0;
	//CDotNetHandle exception ;
	//char *strVal="";
	int isCheck=-1;

	switch (event)
	{
		case EVENT_COMMIT:
			//chec avilable p.unit, acc.unit, d.roata

			//ExecuteAnumCmd((int)6,strVal,(int)0,(int)0);
			//check for tcs no

			GetCtrlVal (panel, control, &isCheck);
			ExecuteAnumCmd((int)7,"",isCheck,(int)0,"");

			break;
	}

	return 0;
}


int CVICALLBACK BoutonExecution (int panel, int control, int event,
								 void *callbackData, int eventData1, int eventData2)
{
	char sTpsEcoule[50];
	//int iErr;
	int rowNo=0;
	CDotNetHandle exception;
	char *strVal="";
	int NbRow_scriptsequence = 0;
	char transScriptDirectory[MAX_PATHNAME_LEN]="";
	char originalScriptDirectory[MAX_PATHNAME_LEN]="";  
	char ProjectDirectory[MAX_PATHNAME_LEN]; 

	SYSTEMTIME t; // Declare SYSTEMTIME struct
	char seqname[50];
	int r = 0;

	
	switch (event)
	{
		case EVENT_LEFT_CLICK:
			
			//Modif MaximePAGES 11/08/2020 - check if the current sequence is saved or not **********
			//Modif MarvynPANNETIER	17/08/2021 
			if (execonf == 1)
			{
			LoadConfiguration(txtseqfile);	
			}
			execonf =1;
			
			
			if (firstCurrentSeqSaved == 1)
			{
				
				GetProjectDir(ProjectDirectory); 
				int result = checkCurrentSeqSaved(ProjectDirectory);

				if (result != 0) 
				{
					MessagePopup("Warning","Please save the current sequence before execution.");
					CallCtrlCallback(GiPanel, PANEL_MODE_SAVEFILEBUTTON, EVENT_COMMIT, 0, 0, &r);
					return -1;
							
				}
				
				
			}
			else 
			{
				MessagePopup("Warning","Please save the current sequence before execution.");
				CallCtrlCallback(GiPanel, PANEL_MODE_SAVEFILEBUTTON, EVENT_COMMIT, 0, 0, &r);
				return -1;
				
			}
			
			// ***************************************************************************************

			//Modif MaximePAGES 21/08/2020
			SetCtrlAttribute (GiPanel, PANEL_MODE_CLOCK_ANALYSE, ATTR_LABEL_TEXT, "Loading..."); 
			SetCtrlAttribute(GiPanel, PANEL_MODE_CLOCK_ANALYSE, ATTR_VISIBLE, 1); 
			
			flag_seegraph = 1;
			SetMenuBarAttribute (GetPanelMenuBar (GiPanel), CONFIG_QUITTER, 			ATTR_DIMMED, 1); 

			SetCtrlAttribute (GiPanel,PANEL_MODE_BUT_START_RUN, ATTR_CMD_BUTTON_COLOR,0x003399FF ); //VAL_BLUE

			//check avilable p.unit, acc.unit, d.roata
			GetNumTableRows (GiPanel, PANEL_MODE_TABLE_SCRIPT, &iNumberOfRows);
			//modif carolina
			NbRow_scriptsequence = iNumberOfRows;
			if(NbRow_scriptsequence <= 0)
			{
				MessagePopup ("Warning", "No Script sequence\nPlease load a Script sequence to execute a test");
			}

			GetCtrlAttribute (GiPanel, PANEL_MODE_BUT_START_RUN, ATTR_LABEL_TEXT, sTpsEcoule); //

			if(strcmp("Execute",sTpsEcoule)!=0)
			{
				SetCtrlAttribute (GiPanel, PANEL_MODE_BUT_START_RUN, ATTR_LABEL_TEXT, "Execute");

				/*SetMenuBarAttribute (GetPanelMenuBar (GiPanel), CONFIG_CONFIG_ITEM_LOAD_CONF, 		ATTR_DIMMED, 	1);  //Modif MaximePAGES 09/09/2020
				SetMenuBarAttribute (GetPanelMenuBar (GiPanel), CONFIG_CONFIG_ITEM_SAVE_CONF, 		ATTR_DIMMED, 	1);
				SetMenuBarAttribute (GetPanelMenuBar (GiPanel), CONFIG_CONFIG_ITEM_LOAD_ANUM_CFG, 	ATTR_DIMMED, 	1);
				SetMenuBarAttribute (GetPanelMenuBar (GiPanel), CONFIG_CONFIG_ITEM_LOAD_DATABASE, 	ATTR_DIMMED, 	1);
				SetMenuBarAttribute (GetPanelMenuBar (GiPanel), CONFIG_CONFIG_ITEM_SET_LOG_DIR, 	ATTR_DIMMED, 	1);   */

				ExecuteAnumCmd(dSTOP,"",0,0,"");

				SetGiStartCycleRun(0);
				SetGiStartCycleVitesse(0);
				SetGiStartCyclePression(0);
				SetGiStartCycleAnum(0);

				return 0;
			}

			//check cfg paths
			if(!CheckPrecondition())
			{
				return 0;
			}

			//Load Anum .cfg
			//ExecuteAnumCmd(6,"Bla",0,0);
			ExecuteAnumCmd((int)6,strVal,(int)0,(int)0,"");
			//check for tcs no
			GetNumTableRows(GiPanel, PANEL_MODE_TABLE_SCRIPT,&rowNo); //Nb rows = nb of tests script

			if(rowNo<=0)
			{
				return 0;    ///
			}

			//Delete Status of previous Runs
			int i=0;
			Point cell;

			for(i = 1; i<= rowNo; i++)
			{
				cell.x = 2;
				cell.y = i;
				SetTableCellVal (GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, "");
				SetTableCellAttribute(GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, ATTR_TEXT_BGCOLOR , VAL_WHITE);
			}

			//***
			
			/*
			//Create or check if the Results directory exists
			strcpy(GsResultsDirPath,GsLogPathInit); //Modif MaximePAGES 21/07/2020 
			strcat(GsResultsDirPath, "\\Results");
			MakeDir(GsResultsDirPath);
			
			*/
			
			//Init interface
			GetLocalTime(&t); // Fill out the struct so that it can be used
			sprintf(seqname,"\\Seq_%02d-%02d-%d_%02d-%02d-%02d",t.wDay,t.wMonth,(t.wYear-2000),t.wHour,t.wMinute,t.wSecond);

			strcpy(GsLogPath,GsLogPathInit); //Modif MaximePAGES 21/07/2020
			strcat(GsLogPath, seqname); //Modif MaximePAGES 21/07/2020
			MakeDir(GsLogPath);		   //Modif MaximePAGES 21/07/2020

			//Modif MaximePAGES 03/08/2020 - directory for translated script test in current test directory  
			sprintf(transScriptDirectory,"%s\\_ Translated scripts",GsLogPath);
			MakeDir(transScriptDirectory);		   
			
			//Modif MaximePAGES 03/08/2020 - directory for original script test  in current test directory 
			sprintf(originalScriptDirectory,"%s\\_ Original scripts",GsLogPath);
			MakeDir(originalScriptDirectory);		   
			
			

			//***

			WuAutoCheck_WU_Automation_Test_and_Folder_Manager_InitInterface(instanceLseNetModes,GsPathApplication,GsLogPath,GsCheckCfgFile,GsAnumCfgFile,GetGiUnitPression(),GetGiUnitVitesse(),GdCoeffAG,&exception);

			//MODIF MaximePAGES 23/06/2020  - E002
			insertElement(myListProcExcelPID,returnLastExcelPID());   //we acquire the PIED Excel process
			//************************************************************************

			testInSeqCounter = 0; //Modif MaximePAGES 15/07/2020
			generateCoverageMatrix("",0); //on initialise l'excel Seq Results

			// Load ANum Config
			LoadANumConfiguration(GsAnumCfgFile);

			//LoadTest(str,panel);
			GetCtrlAttribute (GiPanel, PANEL_MODE_BUT_START_RUN, ATTR_LABEL_TEXT, sTpsEcoule);
			if(strcmp("Execute",sTpsEcoule)==0)// && GetCtrlAttribute(GPanel, PANEL_MODE_START_ENABLE,ATTR_VALUE,))
			{
				//during the test, disable menu bar of the test sequence
				/*SetMenuBarAttribute (GetPanelMenuBar (GiPanel), CONFIG_CONFIG_ITEM_LOAD_CONF, 		ATTR_DIMMED, 	1); //Modif MaximePAGES 09/09/2020
				SetMenuBarAttribute (GetPanelMenuBar (GiPanel), CONFIG_CONFIG_ITEM_SAVE_CONF, 		ATTR_DIMMED, 	1);
				SetMenuBarAttribute (GetPanelMenuBar (GiPanel), CONFIG_CONFIG_ITEM_LOAD_ANUM_CFG, 	ATTR_DIMMED, 	1);
				SetMenuBarAttribute (GetPanelMenuBar (GiPanel), CONFIG_CONFIG_ITEM_LOAD_DATABASE, 	ATTR_DIMMED, 	1);
				SetMenuBarAttribute (GetPanelMenuBar (GiPanel), CONFIG_CONFIG_ITEM_SET_LOG_DIR, 	ATTR_DIMMED, 	1);*/
				SetCtrlAttribute (GiPanel,PANEL_MODE_MODE_EXE, 		ATTR_DIMMED, 0);
				SetCtrlAttribute (GiPanel,PANEL_MODE_MODE_CREATION, ATTR_DIMMED, 0);
				SetCtrlAttribute(GiPanel, PANEL_MODE_MODE_ANALYSE, ATTR_DIMMED,0);
				SetCtrlAttribute (GiPanel, PANEL_MODE_BUT_START_RUN, ATTR_LABEL_TEXT, "Stop");
				execonf = 0;
				SetGiStartCycleRun(1);

			}
			else
			{
				SetCtrlAttribute (GiPanel, PANEL_MODE_BUT_START_RUN, ATTR_LABEL_TEXT, "Execute");
				/*SetMenuBarAttribute (GetPanelMenuBar (GiPanel), CONFIG_CONFIG_ITEM_LOAD_CONF, 	ATTR_DIMMED, 	0);//Modif MaximePAGES 09/09/2020
				SetMenuBarAttribute (GetPanelMenuBar (GiPanel), CONFIG_CONFIG_ITEM_SAVE_CONF, 	ATTR_DIMMED, 	0);
				SetMenuBarAttribute (GetPanelMenuBar (GiPanel), CONFIG_CONFIG_ITEM_LOAD_ANUM_CFG, 	ATTR_DIMMED, 	0);
				SetMenuBarAttribute (GetPanelMenuBar (GiPanel), CONFIG_CONFIG_ITEM_LOAD_DATABASE, 	ATTR_DIMMED, 	0);
				SetMenuBarAttribute (GetPanelMenuBar (GiPanel), CONFIG_CONFIG_ITEM_SET_LOG_DIR, 	ATTR_DIMMED, 	0); */
				SetGiStartCycleRun(0);
				SetGiStartCycleVitesse(0);
				SetGiStartCyclePression(0);
				SetGiStartCycleAnum(0);

			}

			
			break;
	}

	return 0;
}


// DEBUT ALGO - mode manual or automatic
//******************************************************************************
//int CVICALLBACK BoutonPression (int panel, int control, int event,
//		void *callbackData, int eventData1, int eventData2)
//******************************************************************************
//	- Paramètres CVI
//
//  - Gestion du bouton de départ/arrêt cycle de pression
//
//  - 0 si OK, sinon code d'erreur
//******************************************************************************
// FIN ALGO

int CVICALLBACK BoutonPression (int panel, int control, int event,
								void *callbackData, int eventData1, int eventData2)
{
	char sTitle[200];
	int iNumberOfRows;
	int iErr;
	int iYes = 0;

	switch (event)
	{
		case EVENT_COMMIT:
			// Si le cycle de pression est en cours
			if (GetGiStartCyclePression() == 1)
				iYes = ConfirmPopup ("Warning", "Are you sure you want to stop Test?");

			if (GetGiStartCyclePression() == 1)
			{
				if(iYes)
				{
					// Arrêt de la tâche d'acquisition
					//SetAsyncTimerAttribute (GiThreadAcquisitions, ASYNC_ATTR_ENABLED, 0);

					iErr = GstIhmCycles(panel,GetGiStartVitPress(),  GetGiStartCycleVitesse(), 0);

					SetGiStartCyclePression(0);
				}
			}
			else
			{
				if (GetGiMode() == iMODE_AUTOMATIQUE)
				{
					// Détermination du nombre de lignes du tableau des pressions
					iErr = GetNumTableRows (panel, PANEL_MODE_TABLE_PRESSION, &iNumberOfRows);

					if(strlen(GsPathConfigFile) == 0)
					{
						iErr = -1;
						CHECK_ERROR(iErr, "Error", sMSG_SAVE_DATA_ON_FILE)
						return 0;
					}
				}
				else
					iNumberOfRows = 2;

				if (iNumberOfRows >= 2)
				{
					// Lancement du cycle de pression
					SetGiStartCyclePression(1);

					// Initialisations
					SetGdPression(0.0);
					SetGdDurEcoulEssaiPress(0.0);
					SetGiIndexStepPression(1);
					SetGiIndexCyclePression(1);

					// Lancement de la tâche d'acquisition
					SetAsyncTimerAttribute (GiThreadAcquisitions, ASYNC_ATTR_ENABLED, 1);
				}
				else
				{
					// On ne peut pas lancer le cycle de pression
					ModesAddMessageZM(sMSG_CANT_START_PRESS_CYCLE);
				}
				iErr = GstIhmCycles(panel, GetGiStartVitPress(), GetGiStartCycleVitesse(), GetGiStartCyclePression());
			}
			break;
	}
	return 0;
}

void OnBoutonCyclesVitessePress(int panel)
{
	char sTitle[200];
	int iNumberOfRowsPress;
	int iNumberOfRowsSpeed;
	int iErr;
	int iYes = 0;

	// Si les cycles de vitesse et pression sont en cours
	//Sleep(1000);

	if ((GetGiStartCycleVitesse() == 1) || (GetGiStartCyclePression() == 1) || (GetGiStartVitPress() == 1))
		iYes = ConfirmPopup ("Warning", "Are you sure you want to stop Test?");

	if (GetGiStartVitPress() == 1)
	{
		if(iYes)
		{
			iErr = GstIhmCycles(panel,0,  0, 0);

			// Arrêt de la tâche d'acquisition
			//SetAsyncTimerAttribute (GiThreadAcquisitions, ASYNC_ATTR_ENABLED, 0);

			SetGiStartCycleVitesse(0);
			SetGiStartCyclePression(0);
			SetGiStartVitPress(0);
		}
	}
	else if ((GetGiStartCycleVitesse() == 0) && (GetGiStartCyclePression() == 0) && (GetGiStartVitPress() == 0))
	{
		if ((GetGiMode() == iMODE_AUTOMATIQUE)||(GetGiMode() == iMODE_RUN))
		{
			if(strlen(GsPathConfigFile) == 0)
			{
				iErr = -1;
				CHECK_ERROR(iErr, "Error", sMSG_SAVE_DATA_ON_FILE)
				//return 0;
			}

			// Détermination du nombre de lignes du tableau des pressions
			iErr = GetNumTableRows (panel, PANEL_MODE_TABLE_PRESSION, &iNumberOfRowsPress);
			// Détermination du nombre de lignes du tableau des vitesses
			iErr = GetNumTableRows (panel, PANEL_MODE_TABLE_VITESSES, &iNumberOfRowsSpeed);
		}
		else
		{
			iNumberOfRowsPress = 2;
			iNumberOfRowsSpeed = 2;
		}

		// Si le nombre de lignes des deux tableaux est supérieur à deux
		if ((iNumberOfRowsPress >= 2) && (iNumberOfRowsSpeed >= 2))
		{
			// Commande de la ventilation forcée avant de lancer un cycle moteur
			SetGiErrUsb6008(Usb6008CommandeVentilationMoteur (iUSB6008_CMDE_VENTILATION_MOTEUR_ACTIFS));

			// Initialisations
			SetGdDurEcoulEssaiSpeed(0.0);
			SetGdDurEcoulEssaiPress(0.0);
			SetGiIndexStepPression(1);
			SetGiIndexCyclePression(1);
			SetGiIndexStepVitesse(1);
			SetGiIndexCycleVitesse(1);

			SetGiStartCycleVitesse(1);
			SetGiStartCyclePression(1);
			SetGiStartVitPress(1);

			// Lancement de la tâche d'acquisition
			SetAsyncTimerAttribute (GiThreadAcquisitions, ASYNC_ATTR_ENABLED, 1);
		}
		else
		{
			// On ne peut pas lancer le cycle de pression
			if (iNumberOfRowsPress < 2)
			{
				ModesAddMessageZM(sMSG_CANT_START_PRESS_CYCLE);
			}

			// On ne peut pas lancer le cycle de vitesse
			if (iNumberOfRowsSpeed < 2)
			{
				ModesAddMessageZM(sMSG_CANT_START_SPEED_CYCLE);
			}
		}

		iErr = GstIhmCycles(panel, GetGiStartVitPress(), GetGiStartCycleVitesse(), GetGiStartCyclePression());
	}

}
// DEBUT ALGO
//******************************************************************************
//int CVICALLBACK BoutonCyclesVitessePress (int panel, int control, int event,
//		void *callbackData, int eventData1, int eventData2)
//******************************************************************************
//	- Paramètres CVI
//
//  - Gestion de l'appui sur le bouton des cycles de vitesse et pression
//
//  - 0 si OK, sinon code d'erreur
//******************************************************************************
// FIN ALGO
int CVICALLBACK BoutonCyclesVitessePress (int panel, int control, int event,
		void *callbackData, int eventData1, int eventData2)
{


	switch (event)
	{
		case EVENT_COMMIT:
			OnBoutonCyclesVitessePress(panel);
			break;
	}
	return 0;
}


// DEBUT ALGO
//******************************************************************************
//int DisplaySegmentActifGraph(int iPanel, int iControlIdTable, int iControlIdGraphe,
//							 int iStep, int iNbMaxSteps, int iNumCycle, int iColor,
//							 int *ptiSelectionHandle)
//******************************************************************************
//	- int iPanel				: Handle du panel
//	  int iControlIdTable   	: Handle vers la table de données utilisée
//	  int iControlIdGraphe  	: Handle vers le graphique à utiliser
//	  int iStep				 	: Numéro de step sélectionné
//	  int iNbMaxSteps			: Nombre maximal de steps
//	  int iNumCycle				: Numéro de cycle en cours
//	  int iColor			 	: Couleur à appliquer au step sélectionné
//	  int *ptiSelectionHandle	: Handle de la sélection (pour effacement futur)
//
//  - Affichage du segment actif sur le graphique
//
//  - 0 si OK, sinon code d'erreur
//******************************************************************************
// FIN ALGO
int DisplaySegmentActifGraph(int iPanel, int iControlIdTable, int iControlIdGraphe,
							 int iStep, int iNbMaxSteps, int iNumCycle, int iColor,
							 int *ptiSelectionHandle)
{
	Point pCell1;
	Point pCell2;
	double dDuree;
	double dValY;
	double dX1;
	double dY1;
	double dX2;
	double dY2;
	int i;

	dDuree = 0.0;
	pCell1.x = 1;
	pCell1.y = iStep;
	pCell2.x = 2;
	// Ajout de toutes les durées
	for(i=1; i<=pCell1.y; i++)
	{
		pCell2.y = i;
		GetTableCellVal (iPanel, iControlIdTable, pCell2, &dValY);
		dDuree += dValY;
	}

	//---- Colonne des valeurs (vitesse ou pression) ------
	pCell1.x = 1;
	pCell1.y = iStep;
	// Lecture de la valeur en Y de la cellule sélectionnée
	GetTableCellVal (iPanel, iControlIdTable, pCell1, &dValY);

	// Effacement de la dernière sélection sur le graphique
	if (*ptiSelectionHandle != 0)
	{
		DeleteGraphPlot (iPanel, iControlIdGraphe, *ptiSelectionHandle, VAL_IMMEDIATE_DRAW);

		//modif carolina
		if(flag_seegraph == 1)
		{
			DeleteGraphPlot (GiGraphPanel, iControlIdGraphe, *ptiSelectionHandle, VAL_IMMEDIATE_DRAW);
		}
	}

	dX2 = dDuree;
	dY2 = dValY;

	pCell1.x = 2;
	// Lecture de la valeur en Y de la cellule sélectionnée
	GetTableCellVal (iPanel, iControlIdTable, pCell1, &dValY);


	dX1 = dDuree - dValY;

	// Colonne des données (vitesse ou pression)
	pCell1.x = 1;

	// Si la valeur de la ligne est correcte
	if (pCell1.y > 1)
	{
		pCell1.y -= 1;
		//CHECK_TAB_INDEX("7", pCell1.y, 0, 5000)
		// Lecture de la valeur en Y de la cellule sélectionnée
		GetTableCellVal (iPanel, iControlIdTable, pCell1, &dValY);
	}
	else
		dValY = 0.0;

	dY1 = dValY;

	if (iStep == 1)
	{
		dX1	= 0;
		dY1	= dY2;
	}

	// S'il y a plus d'un pixel de différence entre le point de départ et le point d'arrivée
	if ((abs(dX2 - dX1) >= 1.0) || (abs(dY2 - dY1)>= 1.0))
	{
		if(iStep == 1)
		{
			// Traitement particulier pour le premier cycle
			if(iNumCycle == 1)
			{
				dY1 = 0.0;
			}
			else
			{
				// Traitement particulier pour les autres cycles
				pCell1.x = 1;
				pCell1.y = iNbMaxSteps;
				// Lecture de la valeur en Y de la cellule sélectionnée
				GetTableCellVal (iPanel, iControlIdTable, pCell1, &dY1);
			}
		}

		// Tracé d'une ligne
		*ptiSelectionHandle = PlotLine (iPanel, iControlIdGraphe, dX1, dY1, dX2, dY2, iColor);

		// Sauvegarde du handle du graphique de la dernière sélection
		SetGiControlGrapheSelection(iControlIdGraphe);

		// Taille du segment de droite = 3 pixels
		SetPlotAttribute (GiPanel, iControlIdGraphe, *ptiSelectionHandle, ATTR_PLOT_THICKNESS, 3);
		SetPlotAttribute (GiPanel, iControlIdGraphe, *ptiSelectionHandle, ATTR_TRACE_COLOR, iColor);
	}

	return 0; // OK
}

//--------------------------------------------


// DEBUT ALGO
//***************************************************************************
//int CalcParamVitesse(double dVitDep, double dVitFin, double dDuree, double *ptdProgVitDep, double *ptdProgVitFin,
//					double *ptDureeProg, BOOL *ptbMarcheAvant, BOOL *ptbRampe, BOOL *ptbAttendreConsAtteinte)
//***************************************************************************
//  - double dVitDep				: Consigne Vitesse de départ (en trs/min)
//	  double dVitFin				: Consigne Vitesse d'arrivée (en trs/min)
//	  double *ptdProgVitDep			: Vitesse de départ à programmer (en trs/min)
//	  double *ptdProgVitFin			: Vitesse d'arrivée à programmer (en trs/min)
//	  double *ptDureeProg			: Durée du step (en secondes)
//	  BOOL *ptbMarcheAvant			: TRUE = Marche avant, sinon,marche arrière
//	  BOOL *ptbRampe				: TRUE: Une rampe doit être générée, sinon,
//										simple programmation de la vitesse
//	  BOOL *ptbAttendreConsAtteinte : TRUE = attendre la levée du flag de consigne atteinte
//
//  - Calcul du paramétrage du moteur pour un step
//
//  - 1 si problème détecté, 0 sinon
//***************************************************************************
// FIN ALGO
int CalcParamVitesse(double dVitDep, double dVitFin, double dDuree, double *ptdProgVitDep, double *ptdProgVitFin,
					 double *ptDureeProg, BOOL *ptbMarcheAvant, BOOL *ptbRampe, BOOL *ptbAttendreConsAtteinte)
{
	double dPente;
	double dCste;
	char sMsg[800];
	double dTimeElapsed;

	if (dVitDep != dVitFin)
		*ptbRampe = 1;
	else
		*ptbRampe = 0;

	// La durée ne doit pas être inférieure à zéro
	if (dDuree < 0.0001)
		dDuree = 0.0001;

	//CHECK_VAL("1", dDuree, 0.0001, 1000.0)

	if (((dVitDep > 0.0) && (dVitFin < 0.0)) || ((dVitDep < 0.0) && (dVitFin > 0.0)))
	{
		// Si les signes de vitesse sont différents
		*ptdProgVitDep = dVitDep;
		*ptdProgVitFin = 0.0;

		dPente = (dVitFin - dVitDep) / dDuree;
		dCste = dVitDep;

		// Contrôle de la pente
		if(fabs(dPente) > 0.0001)
			*ptDureeProg = (- dCste) / dPente;
		else
			*ptDureeProg = dDuree;

		// Contrôle de la valeur de durée
		if(*ptDureeProg < 0.0001)
			*ptDureeProg = 0.0001;
		else if(*ptDureeProg > dDuree)
			*ptDureeProg = dDuree;

		// Il faut attendre que le moteur soit arrêtté avant de continuer
		*ptbAttendreConsAtteinte = 1;
	}
	else
	{
		*ptdProgVitDep 	= dVitDep;
		*ptdProgVitFin 	= dVitFin;
		*ptDureeProg 	= dDuree;
		*ptbAttendreConsAtteinte = 0;
	}

	if ((*ptdProgVitDep >= 0.0) && (*ptdProgVitFin >= 0.0))
		*ptbMarcheAvant = 1;
	else
		*ptbMarcheAvant = 0;

	//---- Modification du 04/04/2013 par CB -- DEBUT -----
	dTimeElapsed = (double)((clock() - GetGiTimeDepartEssaiPress())) / ((double)CLOCKS_PER_SEC);
	sprintf(sMsg, "dTimeElapsed = %.02f,  dVitDep=%.03f, dVitFin=%.03f, dDuree=%.03f,  ptdProgVitDep=%.03f,  ptdProgVitFin=%.03f,ptDureeProg=%.03f,  ptbMarcheAvant=%d,  ptbRampe=%d,  ptbAttendreConsAtteinte=%d",
			dTimeElapsed, dVitDep, dVitFin, dDuree, *ptdProgVitDep, *ptdProgVitFin,
			*ptDureeProg, *ptbMarcheAvant, *ptbRampe, *ptbAttendreConsAtteinte);

	ModesAddMessageZM(sMsg);

	//---- Modification du 04/04/2013 par CB -- FIN -----

	return 0; // OK
}

// DEBUT ALGO
//***************************************************************************
//int ProgRampeVariateur(BOOL bConvert, stConfig *ptstConfig, double dVitDep, double dVitFin,
//						double dDuree, int iUnitVitesse)
//***************************************************************************
//  - Bool *ptwConsigneAtteinte: 1 = attendre atteinte consigne, sinon 0
//	  BOOL bConvert			 : 1 = Appliquer les coefficients de conversion
//	  stConfig *ptstConfig   : Pointeur vers les paramètres de configuration
//	  double dVitDep		 : Vitesse de départ de la rampe
//	  double dVitFin		 : Vitesse de fin de la rampe
//	  double dDuree			 : Durée de la rampe
//	  int iUnitVitesse		 : Unité de vitesse
//
//  - Programmation d'une rampe sur le variateur
//
//  - 1 si problème détecté, 0 sinon
//***************************************************************************
// FIN ALGO
int ProgRampeVariateur(BOOL bAttConsAtteinte, BOOL bConvert, stConfig *ptstConfig, double dVitDep, double dVitFin,
					   double dDuree, int iUnitVitesse)
{
	double dProgVitDep=0;
	double dProgVitFin=0;
	int iErrVaria;
	int iNbEssais;
	char sMsg[1000];
	double dTimeElapsed;

	if(bConvert)
	{
		switch(iUnitVitesse)
		{
			case iUNIT_KM_H:
				dProgVitDep = dVitDep * ptstConfig->dCoeffAKmH + ptstConfig->dCoeffBKmH;
				dProgVitFin = dVitFin * ptstConfig->dCoeffAKmH + ptstConfig->dCoeffBKmH;
				break;
			case iUNIT_G:
				dProgVitDep = (60 * sqrt(fabs(dVitDep) * ptstConfig->dCoeffAG * dGRAVITE)) / (2 * dPI * ptstConfig->dCoeffAG);
				dProgVitFin = (60 * sqrt(fabs(dVitFin) * ptstConfig->dCoeffAG * dGRAVITE)) / (2 * dPI * ptstConfig->dCoeffAG);

				if(dVitDep < 0.0) dProgVitDep *= -1;
				if(dVitFin < 0.0) dProgVitFin *= -1;
				break;
			case iUNIT_TRS_MIN:
				dProgVitDep = dVitDep;
				dProgVitFin = dVitFin;
				break;
		}
	}
	else
	{
		dProgVitDep = dVitDep;
		dProgVitFin = dVitFin;
	}


	iErrVaria = 0;

	//----- Modif du 01/02/2012 par CB ---- DEBUT -------
	if(dDuree < 0.4)
		dDuree = 0.4;
	//----- Modif du 01/02/2012 par CB ---- FIN -------

	iNbEssais = 0;
	do
	{
		if(iNbEssais > 0)
			Delay(0.05);
		iErrVaria = VariateurProgrammationRampe ((float)dProgVitDep, (float)dProgVitFin, dDuree);
		iNbEssais++;
	}
	while ((iErrVaria == 1) && (iNbEssais < iNB_REESSAIS_DIAL_VARIAT));

	//--------- Modification du 04/04/2013 par CB --- DEBUT ----
	dTimeElapsed = (double)((clock() - GetGiTimeDepartEssaiPress())) / ((double)CLOCKS_PER_SEC);
	sprintf(sMsg, "dTimeElapsed=%.02f, bAttConsAtteinte=%d, bConvert=%d Vit dep=%f, Vit Fin=%f, Duree=%f, dVitDep(G)=%f, dVitFin(G)=%f",
			dTimeElapsed, bAttConsAtteinte, bConvert, dProgVitDep, dProgVitFin, dDuree, dVitDep, dVitFin);
	ModesAddMessageZM(sMsg);
	//--------- Modification du 04/04/2013 par CB --- FIN ----

	return iErrVaria;
}

// DEBUT ALGO
//***************************************************************************
// int CmdeVitesseVariateur(stConfig *ptstConfig, double dVitesse, int iSens, int iUnitVitesse)
//***************************************************************************
//  - stConfig *ptstConfig	: Pointeur vers les paramètres de configuration
//	  double dVitesse		: Vitesse à programmer
//	  int iSens				: Sens de rotation du moteur
//	  int iUnitVitesse		: Unité de vitesse utilisée
//
//  - Conversion et programmation de la vitesse à atteindre
//
//  - 1 si problème détecté, 0 sinon
//***************************************************************************
// FIN ALGO
int CmdeVitesseVariateur(stConfig *ptstConfig, double dVitesse, int iSens, int iUnitVitesse)
{
//char sMsg[500];
	double dProgVit=0;
	int iNbEssais;
	int iErrVaria;

	switch(iUnitVitesse)
	{
		case iUNIT_KM_H:
			dProgVit = dVitesse * ptstConfig->dCoeffAKmH + ptstConfig->dCoeffBKmH;
			break;
		case iUNIT_G:
			dProgVit = (60 * sqrt(fabs(dVitesse) * ptstConfig->dCoeffAG * dGRAVITE)) / (2 * dPI * ptstConfig->dCoeffAG);

			if(dVitesse < 0.0)
				dProgVit *= -1;
			break;
		case iUNIT_TRS_MIN:
			dProgVit = dVitesse;
			break;
	}

	iNbEssais = 0;
	do
	{
		if(iNbEssais > 0)
			Delay(0.05);

		if (iSens == iVARIATEUR_MARCHE_AVANT)
			//  Emission consigne de vitesse et mise en route du variateur
			iErrVaria = VariateurCommandeVitesse ((float)dProgVit, iVARIATEUR_MARCHE_AVANT);
		else
			iErrVaria = VariateurCommandeVitesse ((float)dProgVit, iVARIATEUR_MARCHE_ARRIERE);
		iNbEssais++;
	}
	while ((iErrVaria == 1) && (iNbEssais < iNB_REESSAIS_DIAL_VARIAT));

	return iErrVaria;
}

// DEBUT ALGO
//***************************************************************************
// int LectVitesse(stConfig *ptstConfig, float *ptfVitesse, int iUnitVitesse)
//***************************************************************************
//  - stConfig *ptstConfig	: Pointeur vers les paramètres de configuration
//	  float *ptfVitesse		: Vitesse lue (convertie à l'unitée de vitesse de travail)
//	  int iUnitVitesse		: Unité de vitesse utilisée
//
//  - Lecture et conversion de la vitesse lue
//
//  - 1 si problème détecté, 0 sinon
//***************************************************************************
// FIN ALGO
int LectVitesse(stConfig *ptstConfig, float *ptfVitesse, int iUnitVitesse)
{
	float fVitesseLue;
	double dVitesseLue=0;
	int iErrVaria;
	int iNbEssais;
	double dSigne=1.0;


	iNbEssais = 0;
	do
	{
		if(iNbEssais > 0)
			Delay(0.05);

		// Lecture de la vitesse
		iErrVaria = VariateurMesureVitesse(&fVitesseLue);
		iNbEssais++;
	}
	while ((iErrVaria == 1) && (iNbEssais < iNB_REESSAIS_DIAL_VARIAT));


	// Conversion de la vitesse
	switch(iUnitVitesse)
	{
		case iUNIT_KM_H:
			dVitesseLue = (((double)(fVitesseLue)) - ptstConfig->dCoeffBKmH) / ptstConfig->dCoeffAKmH;
			// La valeur est en dehors des limites
			if ((dVitesseLue < ptstConfig->dLimMinKmH*2) || (dVitesseLue > ptstConfig->dLimMaxKmH*2))
				dVitesseLue = *ptfVitesse;
			break;
		case iUNIT_G:
			if(fVitesseLue >= 0.0)
				dSigne = 1.0;
			else
				dSigne = -1.0;

			// Conversion trs/min en trs/secondes
			fVitesseLue = (fVitesseLue * 2 * dPI * ptstConfig->dCoeffAG) / 60.0;
			dVitesseLue = (fVitesseLue * fVitesseLue) / (ptstConfig->dCoeffAG * dGRAVITE);
			// La valeur est en dehors des limites
			if ((dVitesseLue < ptstConfig->dLimMinG*2) || (dVitesseLue > ptstConfig->dLimMaxG*2))
				dVitesseLue = *ptfVitesse;

			dVitesseLue *= dSigne;
			break;
		case iUNIT_TRS_MIN:
			dVitesseLue = (double)fVitesseLue;
			// La valeur est en dehors des limites
			if ((dVitesseLue < ptstConfig->dLimMinTrsMin*2) || (dVitesseLue > ptstConfig->dLimMaxTrsMin*2))
				dVitesseLue = *ptfVitesse;
			break;
	}

	// Mise à jour de la vitesse lue
	*ptfVitesse = (float)dVitesseLue;

	return iErrVaria;
}

// DEBUT ALGO
//****************************************************************************************************
// int ProgPression(stConfig *ptstConfig, double dPression, int iUnitPression)
//****************************************************************************************************
//  - stConfig *ptstConfig   : Pointeur vers les paramètres de configuration
//	  double dPression		 : Pression à programmer
//	  int iUnitPression		 : Unité de pression
//
//  - Conversion et programmation de la pression
//
//  - 1 si problème détecté, 0 sinon
//****************************************************************************************************
// FIN ALGO
int ProgPression(stConfig *ptstConfig, double dPression, int iUnitPression)
{
	double dProgPression=0;

	switch(iUnitPression)
	{
		case iUNIT_KPA:
			dProgPression = dPression * ptstConfig->dCoeffAKpa + ptstConfig->dCoeffBKpa;
			break;
		case iUNIT_BAR:
			dProgPression = dPression;
			break;
		case iUNIT_MBAR:
			dProgPression = dPression * ptstConfig->dCoeffAMbar + ptstConfig->dCoeffBMbar;
			break;
		case iUNIT_PSI:
			dProgPression = dPression * ptstConfig->dCoeffAPsi + ptstConfig->dCoeffBPsi;
			break;
	}

	SetGiErrUsb6008(Usb6008CommandePression (dProgPression));

	return GetGiErrUsb6008();
}

// DEBUT ALGO
//***************************************************************************
// int LectPression(stConfig *ptstConfig, double *ptdPression, int iUnitPression)
//***************************************************************************
//  - stConfig *ptstConfig   : Pointeur vers les paramètres de configuration
//	  double ptdPression	 : Pression lue (convertie à l'unité de pression courante)
//	  int iUnitPression		 : Unité de pression
//
//  - Lecture et conversion de la pression
//
//  - 1 si problème détecté, 0 sinon
//***************************************************************************
// FIN ALGO
int LectPression(stConfig *ptstConfig, double *ptdPression, int iUnitPression)
{
	double dPressionConv=0;
	double dPressionLue=0.0;

	SetGiErrUsb6008(Usb6008MesurePression (&dPressionLue));



	switch(iUnitPression)
	{
		case iUNIT_KPA:
			dPressionConv = (dPressionLue - ptstConfig->dCoeffBKpa) / ptstConfig->dCoeffAKpa;
			// La valeur est en dehors des limites


			if ((dPressionConv < ptstConfig->dLimMinKpa*2) || (dPressionConv > ptstConfig->dLimMaxKpa*2))
				dPressionConv = *ptdPression;
			break;
		case iUNIT_BAR:
			dPressionConv = dPressionLue;
			// La valeur est en dehors des limites
			if ((dPressionConv < ptstConfig->dLimMinBar*2) || (dPressionConv > ptstConfig->dLimMaxBar*2))
				dPressionConv = *ptdPression;
			break;
		case iUNIT_MBAR:
			dPressionConv = (dPressionLue - ptstConfig->dCoeffBMbar) / ptstConfig->dCoeffAMbar;
			// La valeur est en dehors des limites
			if ((dPressionConv < ptstConfig->dLimMinMbar*2) || (dPressionConv > ptstConfig->dLimMaxMbar*2))
				dPressionConv = *ptdPression;
			break;
		case iUNIT_PSI:
			dPressionConv = (dPressionLue - ptstConfig->dCoeffBPsi) / ptstConfig->dCoeffAPsi;
			// La valeur est en dehors des limites
			if ((dPressionConv < ptstConfig->dLimMinPsi*2) || (dPressionConv > ptstConfig->dLimMaxPsi*2))
				dPressionConv = *ptdPression;
			break;
	}

	*ptdPression = dPressionConv;

	return GetGiErrUsb6008();
}

// DEBUT ALGO
//***************************************************************************
// int ConvLimPression(stConfig *ptstConfig, double *ptdPression, int iUnitPression)
//***************************************************************************
//	- double dPression	 	 : Pression lue
//	  int iUnitPression		 : Unité de pression
//
//  - Conversion de lq pression li;ite
//
//  - Pression convertie
//***************************************************************************
// FIN ALGO
double ConvLimPression(double dPression, int iUnitPression)
{
	double dPressionConv=0;

	switch(iUnitPression)
	{
		case iUNIT_KPA:
			dPressionConv = (dPression - 0.0) / 0.01;
			// La valeur est en dehors des limites
			if ((dPressionConv < 0.0) || (dPressionConv > 2000*2.0))
				dPressionConv = dPression;
			break;
		case iUNIT_BAR:
			dPressionConv = dPression;
			// La valeur est en dehors des limites
			if ((dPressionConv < 0) || (dPressionConv > 20*2.0))
				dPressionConv = dPression;
			break;
		case iUNIT_MBAR:
			dPressionConv = (dPression - 0.0) / 0.001;
			// La valeur est en dehors des limites
			if ((dPressionConv < 0.0) || (dPressionConv > 20000*2.0))
				dPressionConv = dPression;
			break;
		case iUNIT_PSI:
			dPressionConv = (dPression - 0.0) / 0.0689479236;
			// La valeur est en dehors des limites
			if ((dPressionConv < 0.0*2) || (dPressionConv > 225.02))
				dPressionConv = dPression;
			break;
	}

	return dPressionConv;
}

// DEBUT ALGO
//****************************************************************************************************
//void LectEtAffichSpeedAndPress(BOOL bReadVit, BOOL bReadPress, int iPanel, int iStatusPanel,
//								stConfig *ptstConfig, float *ptfVitesse, double *ptdPress,
//								int iUnitVitesse, int iUnitPress, int *ptiErrorUsb, int *ptiErrVaria)
//*****************************************************************************************************
//  - BOOL bReadVit   		: 1=Lecture de la vitesse, sinon 0
//	  BOOL bReadPress		: 1=Lecture de la pression, sinon 0
//	  int iPanel			: Handle du panel de déroulement de l'essai
//	  int iStatusPanel		: Handle de status
//	  stConfig *ptstConfig  : Pointeur vers les données de configuration
//	  float *ptfVitesse		: Vitesse lue
//	  double *ptdPress		: Pression lue
//	  int iUnitVitesse		: Unité de vitesse
//	  int iUnitPress		: Unité de pression
//	  int *ptiErrorUsb		: Code d'erreur USB6008
//	  int *ptiErrVaria		: Code d'erreur variateur
//
//  - Lecture de la vitesse et de la pression
//
//  - Aucun
//******************************************************************************************************* *
// FIN ALGO
void LectEtAffichSpeedAndPress(BOOL bReadVit, BOOL bReadPress, int iPanel, int iStatusPanel, int iGraphPanel,
							   stConfig *ptstConfig, float *ptfVitesse, double *ptdPress,
							   int iUnitVitesse, int iUnitPress, int *ptiErrorUsb, int *ptiErrVaria)
{
	// Lecture de la vitesse
	if (bReadVit)
	{
		*ptfVitesse = 0.0;

		// Lecture de la vitesse en temps réel
		*ptiErrVaria = LectVitesse(ptstConfig, ptfVitesse, iUnitVitesse);
#ifdef bSIMULATION
		*ptfVitesse = GetGdVitesseAProg();// + ((double)(rand()*5)) / ((double)RAND_MAX);
#endif
		if(*ptiErrVaria == 0)
		{
			// Affichage de la vitesse sur le panel de status
			SetCtrlVal (iStatusPanel, PANEL_STAT_SPEED, *ptfVitesse);
			// Affichage de la vitesse sur le panel de déroulement de l'essai
			SetCtrlVal (iPanel, PANEL_MODE_AFFICH_VITESSE, *ptfVitesse);

			// Affichage de la vitesse sur le panel graph
			if(flag_seegraph == 1)
			{

				// Modif MaximePAGES 24/07/2020 - E302	 ***************************
				SetCtrlVal (iGraphPanel, PANELGRAPH_SHOW_ACCEL, *ptfVitesse);



				// auto scale
				if ((2 + *ptfVitesse) > 10)
				{
					SetCtrlAttribute(GiPanel, PANEL_MODE_SHOW_ACC, ATTR_MAX_VALUE, 50.0);
				}
				if ((10 + *ptfVitesse) > 50)
				{
					SetCtrlAttribute(GiPanel, PANEL_MODE_SHOW_ACC, ATTR_MAX_VALUE, 100.0);
				}
				if ((10 + *ptfVitesse) > 100)
				{
					SetCtrlAttribute(GiPanel, PANEL_MODE_SHOW_ACC, ATTR_MAX_VALUE, 150.0);
				}
				if ((10 + *ptfVitesse) > 150)
				{
					SetCtrlAttribute(GiPanel, PANEL_MODE_SHOW_ACC, ATTR_MAX_VALUE, 500.0);
				}
				if ((100 + *ptfVitesse) > 500)
				{
					SetCtrlAttribute(GiPanel, PANEL_MODE_SHOW_ACC, ATTR_MAX_VALUE, 1000.0);
				}


				SetCtrlVal (GiPanel, PANEL_MODE_SHOW_ACC, *ptfVitesse);

				// *****************************************************************

			}
		}
	}

	// Lecture de la pression
	if (bReadPress)
	{
		*ptdPress 	= 0.0;

		// Lecture de la pression en temps réel
		*ptiErrorUsb = LectPression(ptstConfig, ptdPress, iUnitPress);


#ifdef bSIMULATION
		*ptdPress = GetGdPressionAProg();// + ((double)(rand()*0.2)) / ((double)RAND_MAX);
#endif
		if(*ptiErrorUsb == 0)
		{
			// Affichage de la pression sur le panel de status
			SetCtrlVal (iStatusPanel, PANEL_STAT_PRESSURE, *ptdPress);
			// Affichage de la pression sur le panel de déroulement de l'essai
			SetCtrlVal (iPanel, PANEL_MODE_AFFICH_PRESS, *ptdPress);

			// Affichage de la pression sur le panel graph
			if(flag_seegraph == 1)
			{

				// Modif MaximePAGES 24/07/2020 - E302	 ***************************
				SetCtrlVal (iGraphPanel, PANELGRAPH_SHOW_PRES, *ptdPress);

				//*ptdPress = *ptdPress * 10;





				// auto scale
				if ((10 + *ptdPress+92.0) > 150)
				{
					SetCtrlAttribute(GiPanel, PANEL_MODE_SHOW_PRESS, ATTR_MAX_VALUE, 250.0);
				}
				if ((10 + *ptdPress+92.0) > 250)
				{
					SetCtrlAttribute(GiPanel, PANEL_MODE_SHOW_PRESS, ATTR_MAX_VALUE, 500.0);
				}
				if ((100 + *ptdPress+92.0) > 500)
				{
					SetCtrlAttribute(GiPanel, PANEL_MODE_SHOW_PRESS, ATTR_MAX_VALUE, 900.0);
				}
				if ((200 + *ptdPress+92.0) > 900)
				{
					SetCtrlAttribute(GiPanel, PANEL_MODE_SHOW_PRESS, ATTR_MAX_VALUE, 1300.0);
				}


				SetCtrlVal (GiPanel, PANEL_MODE_SHOW_PRESS, (*ptdPress +92.0));

				// *****************************************************************

			}
		}
	}
}

// DEBUT ALGO
//***************************************************************************
//void PilotageMoteurModeAuto ()
//***************************************************************************
//  -
//
//  - Pilotage du moteur en mode automatique
//
//  -
//***************************************************************************
// FIN ALGO
void PilotageMoteurModeAuto (BOOL *ptbAttendreConsAtteinte, short *ptwConsigneAtteinte, BOOL *ptbReprogMoteur,
							 double *ptdDureeRampe, int iTimeDepart, int iIndexStepVitesse,
							 double dDureePrevue, double dDureeStep, double dY1Config, double dY2Config, double *ptdProgVitDep,
							 double *ptdProgVitFin, double *ptdDureeProg, BOOL *ptbMarcheAvant, BOOL *ptbRampe)
{
	int iErr;

	// S'il faut attendre d'atteindre la consigne
	if (*ptbAttendreConsAtteinte)
	{
		//  - Détermination consigne vitesse atteinte
		SetGiErrVaria(VariateurConsigneVitesseAtteinte (ptwConsigneAtteinte));

		// Si la consigne est atteinte
		if (*ptwConsigneAtteinte)
		{
			*ptdDureeRampe = dDureePrevue - ((double)((clock() - iTimeDepart)) / ((double)CLOCKS_PER_SEC));
			iErr = CalcParamVitesse(0.0, dY2Config, *ptdDureeRampe,
									ptdProgVitDep, ptdProgVitFin, ptdDureeProg,
									ptbMarcheAvant, ptbRampe, ptbAttendreConsAtteinte);


			if (*ptbRampe)
				//  - Programmation d'une rampe de vitesse
				SetGiErrVaria(ProgRampeVariateur(*ptbAttendreConsAtteinte, 1, GetPointerToGptstConfig(), *ptdProgVitDep, *ptdProgVitFin, *ptdDureeProg, GetGiUnitVitesse()));
			else
				//  - Programmation d'une rampe de vitesse à la vitesse max
				SetGiErrVaria(ProgRampeVariateur(*ptbAttendreConsAtteinte, 0, GetPointerToGptstConfig(), (double)fVIT_DEB_ARRET, (double)fVIT_FIN_ARRET, dDUREE_ARRET, GetGiUnitVitesse()));
			ReleasePointerToGptstConfig();

			if (*ptbMarcheAvant)
				//  - Emission consigne de vitesse et mise en route du variateur
				SetGiErrVaria(CmdeVitesseVariateur(GetPointerToGptstConfig(), *ptdProgVitFin, iVARIATEUR_MARCHE_AVANT, GetGiUnitVitesse()));
			else
				//  - Emission consigne de vitesse et mise en route du variateur
				SetGiErrVaria(CmdeVitesseVariateur(GetPointerToGptstConfig(), *ptdProgVitFin, iVARIATEUR_MARCHE_ARRIERE, GetGiUnitVitesse()));
			ReleasePointerToGptstConfig();
		}
	}
	else
		// S'il faut reprogrammer le moteur
		if (*ptbReprogMoteur)
		{
			iErr = CalcParamVitesse(dY1Config, dY2Config, dDureeStep,
									ptdProgVitDep, ptdProgVitFin, ptdDureeProg,
									ptbMarcheAvant, ptbRampe, ptbAttendreConsAtteinte);

#ifdef bSIMULATION
			if(*ptbRampe)
				SetCtrlAttribute (GetSimuHandle(), PANEL_SIMU_RAMPE_ACCELERATION, ATTR_DIMMED, 0);
			else
				SetCtrlAttribute (GetSimuHandle(), PANEL_SIMU_RAMPE_ACCELERATION, ATTR_DIMMED, 1);
#endif

			if (*ptbRampe)
				//  - Programmation d'une rampe de vitesse
				SetGiErrVaria(ProgRampeVariateur(*ptbAttendreConsAtteinte, 1, GetPointerToGptstConfig(), *ptdProgVitDep, *ptdProgVitFin, *ptdDureeProg, GetGiUnitVitesse()));
			else
				//  - Programmation d'une rampe de vitesse à la vitesse max
				SetGiErrVaria(ProgRampeVariateur(*ptbAttendreConsAtteinte, 0, GetPointerToGptstConfig(), (double)fVIT_DEB_ARRET, (double)fVIT_FIN_ARRET, dDUREE_ARRET, GetGiUnitVitesse()));
			ReleasePointerToGptstConfig();

			if (*ptbMarcheAvant)
				//  - Emission consigne de vitesse et mise en route du variateur
				SetGiErrVaria(CmdeVitesseVariateur(GetPointerToGptstConfig(), *ptdProgVitFin, iVARIATEUR_MARCHE_AVANT, GetGiUnitVitesse()));
			else
				//  - Emission consigne de vitesse et mise en route du variateur
				SetGiErrVaria(CmdeVitesseVariateur(GetPointerToGptstConfig(), *ptdProgVitFin, iVARIATEUR_MARCHE_ARRIERE, GetGiUnitVitesse()));
			ReleasePointerToGptstConfig();

			// Le moteur a été reprogrammé
			*ptbReprogMoteur = 0;
		}
}

//modif carolina
void ProcessAnumCmd(int index)   //this function is called when there is a comand anum in the script
{
	CDotNetHandle exception ;
	int Command;
	unsigned int NumVal;
	char *StrVal;
	char *ID;
	//char *StorageFolder;
	unsigned int Param;

	WuAutoCheck_LFComand__Get__Command(TabAnumCmd[index],&Command,&exception);
	WuAutoCheck_LFComand__Get__StrVal(TabAnumCmd[index],&StrVal,&exception);
	WuAutoCheck_LFComand__Get__NumVal(TabAnumCmd[index],&NumVal,&exception);
	WuAutoCheck_LFComand__Get__Param(TabAnumCmd[index],&Param,&exception);
	WuAutoCheck_LFComand__Get__ID(TabAnumCmd[index],&ID,&exception);

//	printf("Command = _%d_ and StrVal = _%s_ and NumVal = _%d_ and Param = _%d_ and ID = _%s_\n",Command,StrVal,NumVal,Param,ID); 
	ExecuteAnumCmd(Command,StrVal,NumVal,Param,ID); //command anum
	//ExecuteAnumCmd(7,"",0,0,"");       
}

static int CVICALLBACK ThreadCycleAnum (void *functionData)
{
#define dDELAY_THREAD_RUN 		0.2
	double dTimer;
	CDotNetHandle exception ;
	Point cell;
	int i=0;
	double  actTime=0;
	double  dStart=0;

	double Time;


	int nrItems=0;  //number of anum comands in the test script

	while (!GetGbQuitter())
	{
		if (GetGiMode() == iMODE_RUN)
		{
			if (GetGiStartCycleAnum())
			{
				nrItems=GetGiStartCycleAnum();
				if(TabAnumCmd!=NULL )
				{
					i=0;
					dStart=(double)(clock()) / ((double)CLOCKS_PER_SEC);
					while(i<nrItems && GetGiStartCycleAnum())
					{
						actTime=  (double)(clock()) / ((double)CLOCKS_PER_SEC)- dStart;
						WuAutoCheck_LFComand__Get__Time(TabAnumCmd[i],&Time,&exception);

						if(Time<actTime)
						{
							ProcessAnumCmd(i);

							cell.y = 1;
							cell.x = 2;
							i++;
						}

						dTimer = Timer();
						SyncWait(dTimer, dDELAY_THREAD_RUN);
						ProcessSystemEvents();
					}
				}
				SetGiStartCycleAnum(0);
				//SetCtrlAttribute(GiPanel, PANEL_MODE_BUTTON_SEE_GRAPH, ATTR_DIMMED,1);
			}
		}

		dTimer = Timer();
		SyncWait(dTimer, dDELAY_THREAD_RUN);
		ProcessSystemEvents();
	}
	return(0);
}

//***********************************************************************************************
// Modif - Carolina 04/2019
// This function calculates the total excecution time and shows into the ENDTIME_2 indicator
//***********************************************************************************************
void ShowTotalTimeAndTranslateScripts(int panel, int rowNo)
{
	Point cell;
	int boucle = 0;
	int EndBoucle = 0;
	char sCellValue[200]="";
	//char worksheetName[10]="TestCase";
	char range[30];
	int TempoTotal=0;
	int EndFromExcel=0;
	//int rowNo = 0;
	char exeNoTest[10];
	char exeTestXls[255];
	char exeTestXls2[255];	   //laaa
	int exeNo = 0;
	int NbofTests = 1; //there is ate least one test
	double estimatedTimeSeq = 0.0;

	char newExeTestXls[255];
	char copyScriptCurrentTest[255];
	char currentTestName[255];
	char * newCurrentTestName;
	char mycmd[500] = ""; 

	GetNumTableRows(GiPanel, PANEL_MODE_TABLE_SCRIPT,&rowNo);   //rowNo = number of lines
	for(int j=1; j <= rowNo; j++)
	{
		cell.y = j;
		cell.x = 4;
		GetTableCellVal (GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, exeNoTest);  	//nb of tests
		exeNo=atoi(exeNoTest);
		cell.x = 3;
		GetTableCellVal (GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, exeTestXls);


		//Modif MaximePAGES 03/08/2020 - excel script translation parameters into values***********
		cell.x = 1;
		GetTableCellVal (GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, currentTestName);   //name of the current test

		
		//Modif MaximePAGES 26/08/2020 - we copy the orginal excel test to the current test folder
		sprintf(copyScriptCurrentTest,"%s\\_ Original scripts\\%s",GsLogPath,currentTestName); 
		sprintf(mycmd,"cmd.exe /c COPY \"%s\" \"%s\"",exeTestXls,copyScriptCurrentTest);
		WinExec(mycmd,SW_HIDE);
		

		// Modif MaximePAGES 29/07/2020 - new END Time updating with translateScriptExcel function  (updateENDTime() )
		newCurrentTestName = strtok(currentTestName,".");
		sprintf(newExeTestXls,"%s\\_ Translated scripts\\%s_t.xlsx",GsLogPath,newCurrentTestName);

		

		translateScriptExcel(exeTestXls,newExeTestXls);

		cell.x = 3;
		SetTableCellVal (GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, newExeTestXls);

		cell.x = 3;
		GetTableCellVal (GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, exeTestXls2);



		for(NbofTests = 1;  NbofTests <= exeNo; NbofTests++)
		{
			//open excel and copy of the test script in the online table script
			//ExcelRpt_ApplicationNew(VFALSE,&applicationHandleSeq);
			//system("cmd.exe /c TASKLIST /FI \"imagename eq EXCEL.EXE\" /nh /fo csv   >> C:\\Users\\uic80920\\Desktop\\myoutput.txt"); //MODIF MaximePAGES 18/06/2020
			ExcelRpt_WorkbookOpen (applicationHandleProject, exeTestXls2, &workbookHandleSeq);
			ExcelRpt_GetWorksheetFromIndex(workbookHandleSeq, 1, &worksheetHandleSeq);
			ExcelRpt_GetCellValue (worksheetHandleSeq, "I1", CAVT_INT,&EndBoucle); //EndBoucle = Number of lines in the script

			for(boucle=1; boucle <= EndBoucle-1; boucle++)
			{
				//walking through the lines until the end line
				cell.y = boucle;
				cell.x =1;  //A
			}
			cell.x =1; //END line
			sprintf(range,"A%d",boucle+1);
			ExcelRpt_GetCellValue (worksheetHandleSeq, range, CAVT_CSTRING, &sCellValue);
			EndFromExcel = atoi(sCellValue);
			//Calcul of time
			TempoTotal = EndFromExcel + TempoTotal;
		}
	}

	// Modif MaximePAGES 16/07/2020 - Seq Progress Bar **************
	estimatedTimeSeq = (TempoTotal+0.31*60*rowNo);
	//Tend = TempoTotal ;
//	printf("test2 = %f\n",Tend);
//	printf("test = %f\n",estimatedTimeSeq);
	SetCtrlVal(panel,PANEL_MODE_TOTALTIME, estimatedTimeSeq);
	ProgressBar_SetTotalTimeEstimate (GiPanel, PANEL_MODE_PROGRESSBARSEQ, estimatedTimeSeq);
	ProgressBar_Start(GiPanel,PANEL_MODE_PROGRESSBARSEQ,"Sequence Progress Bar:");


	//***************************************************************



	//Fermeture excel
	ExcelRpt_WorkbookClose (workbookHandleSeq, 0);
	//ExcelRpt_ApplicationQuit (applicationHandleSeq);
	//CA_DiscardObjHandle(applicationHandleSeq);
}


char *tabLabel[50];
//***********************************************************************************************
// Modif - Carolina 04/2019
// This function shows the current test script into the panel: PANEL_MODE_SCRIPTS_DETAILS_2
//***********************************************************************************************
void ShowCurrentScript(int panel, char *pathCurrentScript)
{
	Point cell;

	int EndBoucle = 0;
	int boucle = 0;
	char sCellValue[200]="";
	char sTime[200] = "";
	int Time = 0;
	//char worksheetName[10]="TestCase";
	char range[30];
	int EndFromExcel=0;
	char Function[200]="";
	//char auxList[100] = "";
	int numberofLabels = 0;
	int i = 0;
	char *token;
	char *listLabel = Function;

	//open excel and copy of the test script in the online table script
	//ExcelRpt_ApplicationNew(VFALSE,&applicationHandleCurrentScript);
	//system("cmd.exe /c TASKLIST /FI \"imagename eq EXCEL.EXE\" /nh /fo csv  >> C:\\Users\\uic80920\\Desktop\\myoutput.txt"); //MODIF MaximePAGES 18/06/2020
	ExcelRpt_WorkbookOpen (applicationHandleProject, pathCurrentScript, &workbookHandleCurrentScript);
	ExcelRpt_GetWorksheetFromIndex(workbookHandleCurrentScript, 1, &worksheetHandleCurrentScript);
	ExcelRpt_GetCellValue (worksheetHandleCurrentScript, "I1", CAVT_INT, &EndBoucle); //EndBoucle = Number of lines in the script

	for(boucle=1; boucle <= EndBoucle-1; boucle++)
	{
		cell.y = boucle;
		cell.x =3;  //Function
		sprintf(range,"C%d",boucle+1);
		ExcelRpt_GetCellValue (worksheetHandleCurrentScript,range , CAVT_CSTRING,Function);

		if((strcmp(Function, "SetA") && strcmp(Function, "SetP")
				&& strcmp(Function, "LFPower")
				&& strcmp(Function, "SendLFD")
				&& strcmp(Function, "SendLFCw")
				&& strcmp(Function, "StopLFD")
				&& strcmp(Function, "StopLFCw") ) != 0 )
		{
			numberofLabels++;
		}

	}
	/*	if(numberofLabels != 0)
		{
			char nameLabel[numberofLabels];
		}  */
	//initialization(&myPile);
	//initializationTime(&myTime);


	for(boucle=1; boucle <= EndBoucle-1; boucle++)
	{
		InsertTableRows (GiPanel, PANEL_MODE_SCRIPTS_DETAILS_2, -1, 1, VAL_USE_MASTER_CELL_TYPE);

		cell.y = boucle;

		//x number of columns
		cell.x =1;  //Time
		sprintf(range,"A%d",boucle+1);
		ExcelRpt_GetCellValue (worksheetHandleCurrentScript, range, CAVT_CSTRING,sTime);
		SetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS_2,cell, sTime);

		cell.x =2;  //Duration
		sprintf(range,"B%d",boucle+1);
		ExcelRpt_GetCellValue (worksheetHandleCurrentScript,range , CAVT_CSTRING,sCellValue);
		SetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS_2,cell, sCellValue);

		cell.x =3;  //Function
		sprintf(range,"C%d",boucle+1);
		ExcelRpt_GetCellValue (worksheetHandleCurrentScript,range , CAVT_CSTRING, Function);
		SetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS_2,cell, Function);


		if(numberofLabels != 0)
		{
			if((strcmp(Function, "SetA") && strcmp(Function, "SetP")
					&& strcmp(Function, "LFPower")
					&& strcmp(Function, "SendLFD")
					&& strcmp(Function, "SendLFCw")
					&& strcmp(Function, "StopLFD")
					&& strcmp(Function, "StopLFCw") ) != 0 )
			{

				Time = atoi(sTime);
				Time = Time * 1000;
				listLabel = Function; // take the name of function

				//stackup(&myPile, listLabel);
				//stackupTime(&myTime, Time);

				token = Function;
				tabLabel[i] = token;
				i++;
			}
		}

		cell.x =4;  //Value
		sprintf(range,"D%d",boucle+1);
		ExcelRpt_GetCellValue (worksheetHandleCurrentScript,range , CAVT_CSTRING,sCellValue);
		SetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS_2,cell, sCellValue);

		cell.x =5;  //Nb of Frames
		sprintf(range,"E%d",boucle+1);
		ExcelRpt_GetCellValue (worksheetHandleCurrentScript,range , CAVT_CSTRING,sCellValue);
		SetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS_2,cell, sCellValue);

		cell.x =6;  //WU ID
		sprintf(range,"F%d",boucle+1);
		ExcelRpt_GetCellValue (worksheetHandleCurrentScript,range , CAVT_CSTRING,sCellValue);
		//printf("%s\n",sCellValue);
		SetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS_2,cell, sCellValue);

		cell.x =7;  //Interframe
		sprintf(range,"G%d",boucle+1);
		ExcelRpt_GetCellValue (worksheetHandleCurrentScript,range , CAVT_CSTRING,sCellValue);
		SetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS_2,cell, sCellValue);

		cell.x =8;  //Comments
		sprintf(range,"H%d",boucle+1);
		ExcelRpt_GetCellValue (worksheetHandleCurrentScript,range , CAVT_CSTRING,sCellValue);
		SetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS_2,cell, sCellValue);

		//couleurLigne(boucle, panel);
	}
	cell.x =1; //END line
	sprintf(range,"A%d",boucle+1);
	ExcelRpt_GetCellValue (worksheetHandleCurrentScript, range, CAVT_CSTRING, &sCellValue);
	EndFromExcel = atoi(sCellValue);

	currentEndTimeGlobal = (float)EndFromExcel; //MOFID MaximePAGES 1/07/2020 - Script Progress Bar


	//show time in s
	SetCtrlVal(panel,PANEL_MODE_ENDTIMESEQUENCE, (float)EndFromExcel);


	//MODIF MaximePAGES 17/06/2020
//	ExcelRpt_WorkbookClose (workbookHandleCurrentScript, 0);
//	ExcelRpt_ApplicationQuit (applicationHandleCurrentScript);
//	CA_DiscardObjHandle(applicationHandleCurrentScript);



}


//***********************************************************************************************
// Modif - Carolina 04/2019
//
//***********************************************************************************************
char* translateData(char *InterfacetoCheck, char *Parametertocheck)
{
	char range[30] = "";
	char realParameter[255] = "";
	char parameter[255] = "";
	int parameterCompare = 0;
	int boucle = 0, end = 1;

	//ExcelRpt_ApplicationNew(VFALSE,&applicationHandle);
	//ExcelRpt_WorkbookOpen (applicationHandleProject, GsCheckCfgFile, &workbookHandledata);
	//ExcelRpt_GetWorksheetFromIndex(workbookHandledata, 1, &worksheetHandledata1);

	
	
	if (strcmp(InterfacetoCheck, "") == 0) 
	{	
		//Marvyn 04/08/2021
		//load column A and compare
		for(boucle=2; end==1; boucle++) 
		{
			sprintf(range,"%s%d",Interfaces[0].columnParam,boucle+1);  
			ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

			parameterCompare = strcmp(parameter,Parametertocheck);	// compare les deux chaine de caractère
			if(parameterCompare == 0)
			{
				//get the word in the next column
				sprintf(range,"%s%d",Interfaces[0].columnAttribute,boucle+1);
				ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, realParameter);
				end = 0;

				return realParameter;	 
			}
		}
		end=1;
	}
	else if(strcmp(InterfacetoCheck, Interfaces[0].name) == 0)
	{
		//load column A and compare
		for(boucle=2; end==1; boucle++) //A2 until find parameter
		{
			sprintf(range,"%s%d",Interfaces[0].columnParam,boucle+1);  //start with A3
			ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

			parameterCompare = strcmp(parameter,Parametertocheck);	// compare les deux chaine de caractère
			if(parameterCompare == 0)
			{
				//get the word in the next column
				sprintf(range,"%s%d",Interfaces[0].columnAttribute,boucle+1);
				ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, realParameter);
				end = 0;

				return realParameter;	 //Modif MaximePAGES 30/06/2020
			}
		}
		end=1;
	}

	else if(strcmp(InterfacetoCheck, Interfaces[1].name) == 0)
	{
		//load column A and compare
		for(boucle=2; end==1; boucle++) //A2 until find parameter
		{
			sprintf(range,"%s%d",Interfaces[1].columnParam,boucle+1);  //start with A3
			ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

			parameterCompare = strcmp(parameter,Parametertocheck);	// compare les deux chaine de caractère
			if(parameterCompare == 0)
			{
				//get the word in the next column
				sprintf(range,"%s%d",Interfaces[1].columnAttribute,boucle+1);
				ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, realParameter);
				end = 0;

				return realParameter;	 //Modif MaximePAGES 30/06/2020
			}
		}
		end=1;
	}
	else if(strcmp(InterfacetoCheck, Interfaces[2].name) == 0)
	{
		//load column A and compare
		for(boucle=2; end==1; boucle++) //A2 until find parameter
		{
			sprintf(range,"%s%d",Interfaces[2].columnParam,boucle+1);  //start with A3
			ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

			parameterCompare = strcmp(parameter,Parametertocheck);	// compare les deux chaine de caractère
			if(parameterCompare == 0)
			{
				//get the word in the next column
				sprintf(range,"%s%d",Interfaces[2].columnAttribute,boucle+1);
				ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, realParameter);
				end = 0;

				return realParameter;	 //Modif MaximePAGES 30/06/2020
			}
		}
		end=1;
	}
	else if(strcmp(InterfacetoCheck, Interfaces[3].name) == 0)
	{
		//load column A and compare
		for(boucle=2; end==1; boucle++) //A2 until find parameter
		{
			sprintf(range,"%s%d",Interfaces[3].columnParam,boucle+1);  //start with A3
			ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

			parameterCompare = strcmp(parameter,Parametertocheck);	// compare les deux chaine de caractère
			if(parameterCompare == 0)
			{
				//get the word in the next column
				sprintf(range,"%s%d",Interfaces[3].columnAttribute,boucle+1);
				ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, realParameter);
				end = 0;

				return realParameter;	 //Modif MaximePAGES 30/06/2020
			}
		}
		end=1;
	}
	else if(strcmp(InterfacetoCheck, Interfaces[4].name) == 0)
	{
		//load column A and compare
		for(boucle=2; end==1; boucle++) //A2 until find parameter
		{
			sprintf(range,"%s%d",Interfaces[4].columnParam,boucle+1);  //start with A3
			ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

			parameterCompare = strcmp(parameter,Parametertocheck);	// compare les deux chaine de caractère
			if(parameterCompare == 0)
			{
				//get the word in the next column
				sprintf(range,"%s%d",Interfaces[4].columnAttribute,boucle+1);
				ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, realParameter);
				end = 0;

				return realParameter;	 //Modif MaximePAGES 30/06/2020
			}
		}
		end=1;
	}
	else if(strcmp(InterfacetoCheck, Interfaces[5].name) == 0)
	{
		//load column A and compare
		for(boucle=2; end==1; boucle++) //A2 until find parameter
		{
			sprintf(range,"%s%d",Interfaces[5].columnParam,boucle+1);  //start with A3
			ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

			parameterCompare = strcmp(parameter,Parametertocheck);	// compare les deux chaine de caractère
			if(parameterCompare == 0)
			{
				//get the word in the next column
				sprintf(range,"%s%d",Interfaces[5].columnAttribute,boucle+1);
				ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, realParameter);
				end = 0;

				return realParameter;	 //Modif MaximePAGES 30/06/2020
			}
		}
		end=1;
	}
	else if(strcmp(InterfacetoCheck, Interfaces[6].name) == 0)
	{
		//load column A and compare
		for(boucle=2; end==1; boucle++) //A2 until find parameter
		{
			sprintf(range,"%s%d",Interfaces[6].columnParam,boucle+1);  //start with A3
			ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

			parameterCompare = strcmp(parameter,Parametertocheck);	// compare les deux chaine de caractère
			if(parameterCompare == 0)
			{
				//get the word in the next column
				sprintf(range,"%s%d",Interfaces[6].columnAttribute,boucle+1);
				ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, realParameter);
				end = 0;

				return realParameter;	 //Modif MaximePAGES 30/06/2020
			}
		}
		end=1;
	}
	else if(strcmp(InterfacetoCheck, Interfaces[7].name) == 0)
	{
		//load column A and compare
		for(boucle=2; end==1; boucle++) //A2 until find parameter
		{
			sprintf(range,"%s%d",Interfaces[7].columnParam,boucle+1);  //start with A3
			ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

			parameterCompare = strcmp(parameter,Parametertocheck);	// compare les deux chaine de caractère
			if(parameterCompare == 0)
			{
				//get the word in the next column
				sprintf(range,"%s%d",Interfaces[7].columnAttribute,boucle+1);
				ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, realParameter);
				end = 0;

				return realParameter;	 //Modif MaximePAGES 30/06/2020
			}
		}
		end=1;
	}
	else if(strcmp(InterfacetoCheck, Interfaces[8].name) == 0)
	{
		//load column A and compare
		for(boucle=2; end==1; boucle++) //A2 until find parameter
		{
			sprintf(range,"%s%d",Interfaces[8].columnParam,boucle+1);  //start with A3
			ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

			parameterCompare = strcmp(parameter,Parametertocheck);	// compare les deux chaine de caractère
			if(parameterCompare == 0)
			{
				//get the word in the next column
				sprintf(range,"%s%d",Interfaces[8].columnAttribute,boucle+1);
				ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, realParameter);
				end = 0;

				return realParameter;	 //Modif MaximePAGES 30/06/2020
			}
		}
		end=1;
	}
	else if(strcmp(InterfacetoCheck, Interfaces[9].name) == 0)
	{
		//load column A and compare
		for(boucle=2; end==1; boucle++) //A2 until find parameter
		{
			sprintf(range,"%s%d",Interfaces[9].columnParam,boucle+1);  //start with A3
			ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

			parameterCompare = strcmp(parameter,Parametertocheck);	// compare les deux chaine de caractère
			if(parameterCompare == 0)
			{
				//get the word in the next column
				sprintf(range,"%s%d",Interfaces[9].columnAttribute,boucle+1);
				ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, realParameter);
				end = 0;

				return realParameter;	 //Modif MaximePAGES 30/06/2020
			}
		}
		end=1;
	}
	else if(strcmp(InterfacetoCheck, Interfaces[10].name) == 0)
	{
		//load column A and compare
		for(boucle=2; end==1; boucle++) //A2 until find parameter
		{
			sprintf(range,"%s%d",Interfaces[10].columnParam,boucle+1);  //start with A3
			ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

			parameterCompare = strcmp(parameter,Parametertocheck);	// compare les deux chaine de caractère
			if(parameterCompare == 0)
			{
				//get the word in the next column
				sprintf(range,"%s%d",Interfaces[10].columnAttribute,boucle+1);
				ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, realParameter);
				end = 0;

				return realParameter;	 //Modif MaximePAGES 30/06/2020
			}
		}
		end=1;
	}
	else if(strcmp(InterfacetoCheck, Interfaces[11].name) == 0)
	{
		//load column A and compare
		for(boucle=2; end==1; boucle++) //A2 until find parameter
		{
			sprintf(range,"%s%d",Interfaces[11].columnParam,boucle+1);  //start with A3
			ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

			parameterCompare = strcmp(parameter,Parametertocheck);	// compare les deux chaine de caractère
			if(parameterCompare == 0)
			{
				//get the word in the next column
				sprintf(range,"%s%d",Interfaces[11].columnAttribute,boucle+1);
				ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, realParameter);
				end = 0;

				return realParameter;	 //Modif MaximePAGES 30/06/2020
			}
		}
		end=1;
	}
	else if(strcmp(InterfacetoCheck, Interfaces[12].name) == 0)
	{
		//load column A and compare
		for(boucle=2; end==1; boucle++) //A2 until find parameter
		{
			sprintf(range,"%s%d",Interfaces[12].columnParam,boucle+1);  //start with A3
			ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

			parameterCompare = strcmp(parameter,Parametertocheck);	// compare les deux chaine de caractère
			if(parameterCompare == 0)
			{
				//get the word in the next column
				sprintf(range,"%s%d",Interfaces[12].columnAttribute,boucle+1);
				ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, realParameter);
				end = 0;

				return realParameter;	 //Modif MaximePAGES 30/06/2020
			}
		}
		end=1;
	}
	else if(strcmp(InterfacetoCheck, Interfaces[13].name) == 0)
	{
		//load column A and compare
		for(boucle=2; end==1; boucle++) //A2 until find parameter
		{
			sprintf(range,"%s%d",Interfaces[13].columnParam,boucle+1);  //start with A3
			ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

			parameterCompare = strcmp(parameter,Parametertocheck);	// compare les deux chaine de caractère
			if(parameterCompare == 0)
			{
				//get the word in the next column
				sprintf(range,"%s%d",Interfaces[13].columnAttribute,boucle+1);
				ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, realParameter);
				end = 0;

				return realParameter;	 //Modif MaximePAGES 30/06/2020
			}
		}
		end=1;
	}


	//ExcelRpt_WorkbookClose (workbookHandle, 0);
	//CA_DiscardObjHandle(applicationHandle);
	//ExcelRpt_ApplicationQuit (applicationHandle);

	return 0;
}


//***********************************************************************************************
// Modif - Carolina 04/2019
// Separate string in 2 labels
char label1[255]="";

char label2[255]="";
int Time1 = 0;
int Time2 = 0;
char range1[255] = "";
char range2[255] = "";
int lab1=0;
int lab2=0;
//***********************************************************************************************
//MARVYN Modif 02/07/2021 : re implementation de la fonction d'une autre manière
void RetrieveandSeparateLabel(char *label)
{

	char caracter[] = " |";
	char *token;
	char *tab[2]={NULL};
	int i=0;
	lab1 =0;
	lab2=0;
//	int index=0;
	strcpy(label1,"");
	strcpy(label2,"");
	
	if (label[0] == ' ' && label[1] == '|')
	{
	//strcpy(label1,"");
	strcpy(label2,strtok(label,caracter));
	label2[strlen(label2)-1]=NULL;
	lab1 = 1;
	TwoLabs = 0;
	FirstLab = 0;
	}	
	else
	{
	token = strtok(label,caracter);
	while (token != NULL)
	    {
		//printf(" %s\n ",token);
		tab[i]=token;
		token = strtok(NULL, caracter);
		i++;
		}
	strcpy(label1, tab[0]);
	strcpy(label2, tab[1]);
	label2[strlen(label2)-1]=NULL;
	lab1=0;
	}
	if (strcmp(label2,"") == 0) lab2=1;
	if (lab1==0 && lab2==0) TwoLabs=1;
	if (lab1==1 && lab2==0) FirstLab=1;
	//printf("\n\n retrieve label1 = __%s__ and label2 = __%s__\n\n",label1,label2);


	
	
}

//***********************************************************************************************
// Modif - Carolina 04/2019
// separating range in two numbers - range require
// ex 0-4; range1 = 0 and range2 = 4
//***********************************************************************************************
void RetrieveandSeparateRange(char *Valuebis)
{
	char Value[100]="";
	strcpy(Value,Valuebis);
	
	char caracter2[] = ":;";
	char *token2 = strtok(Value, caracter2);
	char *tabtoken[2] = {"",""};
	int i = 0;

	if(token2 != NULL)
	{
		while(token2 != NULL)
		{
			tabtoken[i]=token2;
			token2 = strtok(NULL, caracter2);
			i++;
		}
	}

	strcpy(range1, tabtoken[0]);
	strcpy(range2, tabtoken[1]);

	strcat(range1, ";");
	strcat(range2, ";");
}

//***********************************************************************************************
// Modif - Carolina 04/2019
// counting number of values - sequence require
//***********************************************************************************************
int conta(char *str)
{

	int i=0;
	int n = 0;
	char auxstr[100] = "";
	strcpy(auxstr, str);

	while(auxstr[i]!= '\0')
	{
		if (auxstr[i] == ';')
		{
			n++;
		}
		i++;
	}

	return n;

}


// Modif MaximePAGES 04/08/2020 ******************************************
//return 1 if the value is signel value or sequence
//return 2 if the value is range
int typeOfValue(char *str)
{

	int i=0;
	int n = 0;
	char auxstr[100] = "";
	strcpy(auxstr, str);
	int found = 0;

	while((auxstr[i]!= '\0') && (found == 0))
	{
		if (auxstr[i] == ';')
		{
			n = 1;
			found = 1;
		}

		if (auxstr[i] == ':')
		{
			n = 2;
			found = 1;
		}
		i++;
	}
	return n;
}



//***********************************************************************************************
// Modif - Carolina 04/2019
// Take number of frames from database
//***********************************************************************************************
int TranslateFrameNb()
{
	char range[30] = "";
	//char endCondition[2] = "";
	char parameter[255] = "";
	char sparam[255] = "";
	int parameterCompare = 0;
	int boucle = 0, end = 1;
	//int counterLines = 0;
	char FrameNb[20] = "Frame number";
	//char FramesCount[20] = "FramesCount";

	int res = findColumn("Param Number"); 
	//Database - Configuration sheet
	//ExcelRpt_ApplicationNew(VFALSE,&applicationHandle);
	//ExcelRpt_WorkbookOpen (applicationHandleProject, GsCheckCfgFile, &workbookHandledata);
	//ExcelRpt_GetWorksheetFromIndex(workbookHandledata, 1, &worksheetHandledata1);

	//if(strcmp(InterfacetoCheck, "Stand_Frm") == 0)
	//{
	for(boucle=2; end==1; boucle++)
	{
		
		sprintf(range,"%s%d",col.columnParam,boucle+1);  // Column M -> take value in N
		ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

		parameterCompare = strcmp(parameter,FrameNb);
		if(parameterCompare == 0)
		{
			end = 0;
			sprintf(range,"%s%d",col.columnAttribute,boucle+1);
			ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, sparam);
			return atoi(sparam); //Ex: return "0"
		}
	}
	MessagePopup ("Warning", "The Frame number line is missing in the database, add it in the column \"Param number\"");
	end=1;
	//ExcelRpt_WorkbookClose (workbookHandle, 0);
	return 0;
}



//***********************************************************************************************
// Modif - Carolina 04/2019
// take number of angles emisson from database
//***********************************************************************************************
int TranslateAnglePosition()
{
	char range[30] = "";
	//char endCondition[2] = "";
	char parameter[255] = "";
	char sparam[255] = "";
	int parameterCompare = 0;
	int boucle = 0, end = 1;
	char Angle_position[40] = "Number of emission angles for LSE";


	//ExcelRpt_ApplicationNew(VFALSE,&applicationHandle);
	//ExcelRpt_WorkbookOpen (applicationHandleProject, GsCheckCfgFile, &workbookHandledata);
	//ExcelRpt_GetWorksheetFromIndex(workbookHandledata, 1, &worksheetHandledata1);

	for(boucle=2; end==1; boucle++)
	{
		sprintf(range,"M%d",boucle+1);  //
		ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

		parameterCompare = strcmp(parameter,Angle_position);
		if(parameterCompare == 0)
		{
			end = 0;
			sprintf(range,"N%d",boucle+1);
			ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, sparam);
			return atoi(sparam); //return "4"
		}
	}
	end=1;
	//ExcelRpt_WorkbookClose (workbookHandle, 0);
	return 0;
}

//MArvyn PANNETIER 16/07/2021
// Function that will search if the label is already insert in the logs.xlsx file 
//return 1 if label already exists
int labelAlreadyInsert(char *label)
{
	int i = 0;
	int find = 0;
	//printf("label[i] = %s\n",labelHistory[i]);
	while ( strcmp(labelHistory[i],"")!=0 && find == 0 )
	{
	//	printf("i = %d and lab = __%s__ and labelHistory = __%s__\n",i, label,labelHistory[i]);
		if (strcmp(label,labelHistory[i])==0 && label != NULL)
		{
			find = 1;
		}
		else
		{
			i++;
			
		}
	//	printf("i=%d\n",i);
	}
	return find;
}

//MARVYN 23/07/2021
/*int labelAlreadyExists(char *label)
{
	int i = 0;
	int find = 0;
//	printf("label[i] = %s\n",labelsInsert[i]);
	while ( strcmp(labelsInsert[i],"")!=0 && find == 0 )
	{
		//printf("i = %d and lab = __%s__ and labelsInsert = __%s__\n",i, label,labelsInsert[i]);
		if (strcmp(label,labelsInsert[i])==0 && label != NULL)
		{
			find = 1;
		}
		else
		{
			i++;
			
		}
		//printf("i=%d\n",i);
	}
	return find;
} */
	




//int CKSGoodAlways(char *sProjectDir);
//***********************************************************************************************
// Modif - Carolina 04/2019
// find time that correspond to the label and insert labels into Logs.xlsx
//***********************************************************************************************
int InsertTwoLabel()
{
	int boucle = 0, end = 1;
	char TimeColumn[255] = "";
	char range[30] = "";
	char rangeA[30] = "";
	char rangeA1[30] = "";
	char rangeA2[30] = "";
	char searchData[15] = "/ROW/@_RTime";
	char foundCellRangeTime[50]="";
	int intTimeColumn = 0;
	char cellTime[255] = "";
	char AddLabel1_condition[255] = "";
	char AddLabel2_condition[255] = "";
	int label2Placed = 0, label1Placed = 0;
	int rowAt1000 = 0;


	//ExcelRpt_ApplicationNew(VTRUE,&applicationHandle9); //needs to be VFALSE
	ExcelRpt_WorkbookOpen (applicationHandleProject, fichierLogxml, &workbookHandle9);
	ExcelRpt_GetWorksheetFromIndex(workbookHandle9, 1, &worksheetHandle9);

	//find Time 1 and Time 2 -> linge 2 /ROW/@_RTime and "_RTime" is
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, searchData, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,foundCellRangeTime);
	char caracter2[] = "$";
	char *token2 = strtok(foundCellRangeTime, caracter2);
	if(token2 == NULL)
	{
		fprintf(file_handle, "////////////////////////////////////////////////////////////\n");
		fprintf(file_handle, "Error during insert label!\n\n");
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", searchData);
		fprintf(file_handle, "NOTE : Current directory path is probably too long (more than 256 characters, Windows 10 limitation).\n\n");
		fprintf(file_handle, "////////////////////////////////////////////////////////////\n");


	}
	else
		strcpy(cellTime, token2);


	//Modif MaximePAGES 27/07/2020 - to avoid an infinite loop
	if (Time1 == 0)
	{
		Time1=1000;
		fprintf(file_handle, "Problem into InsertTwoLabel() function : time of Label1 = 0.Set by default at 1000 ms.\n");

	}



	if(token2 != NULL)
	{
		if((Time1 !=0) && (Time2 != 0))
		{
			//parcourir column to find time
			for(boucle=2; end==1; boucle++)
			{
				sprintf(range,"%s%d",cellTime, boucle+1);
				ExcelRpt_GetCellValue (worksheetHandle9, range, CAVT_CSTRING, TimeColumn);
				intTimeColumn = atoi(TimeColumn);

				//we save the RTime at 1000 for later
				if (intTimeColumn == 1000)
				{
					rowAt1000 = boucle+1;
				}


				if(intTimeColumn == Time1)
				{
					sprintf(rangeA1, "A%d", boucle);
					ExcelRpt_GetCellValue (worksheetHandle9, rangeA1, CAVT_CSTRING, AddLabel1_condition);

					if(strcmp(AddLabel1_condition, label1) != 0)
					{
						label1Placed = 1; //Modif MaximePAGES 28/07/2020
						ExcelRpt_InsertRow(worksheetHandle9, boucle+1, 1);
						sprintf(rangeA, "A%d", boucle+1);
						ExcelRpt_SetCellValue (worksheetHandle9, rangeA, CAVT_CSTRING , label1);
						boucle=boucle+1;
					}
				}
				if(intTimeColumn == Time2)
				{
					sprintf(rangeA2, "A%d", boucle);
					ExcelRpt_GetCellValue (worksheetHandle9, rangeA2, CAVT_CSTRING, AddLabel2_condition);

					if(strcmp(AddLabel2_condition, label2) != 0)
					{
						label2Placed = 1;   //Modif MaximePAGES 23/07/2020
						ExcelRpt_InsertRow(worksheetHandle9, boucle+1, 1);
						sprintf(rangeA, "A%d", boucle+1);
						ExcelRpt_SetCellValue (worksheetHandle9, rangeA, CAVT_CSTRING , label2);
						end=0;
					}
					else
					{
						end=0;
					}

				}

				//Modif MaximePAGES 23/07/2020 - to avoid an infinite loop
				sprintf(rangeA2, "A%d", boucle);
				ExcelRpt_GetCellValue (worksheetHandle9, rangeA2, CAVT_CSTRING, AddLabel2_condition);

				//if the label2 is not placed
				if ( (strcmp(AddLabel2_condition,"") == 0) &&  (label2Placed == 0) )
				{
					label2Placed = 1;
					//printf("label2 = %s\n",label2  );
					fprintf(file_handle, "\nProblem into InsertTwoLabel() function : label 2 time not found. Set by default at the end.\n");
					ExcelRpt_SetCellValue (worksheetHandle9, rangeA2, CAVT_CSTRING , label2);
					end=0;
				}

				//if the label1 is not placed
				if ( (label2Placed == 1) &&  (label1Placed == 0) )
				{
					fprintf(file_handle, "Problem into InsertTwoLabel() function : label 1 time not found. Set by default at 1000 ms.");
					sprintf(rangeA1, "A%d", rowAt1000);
					ExcelRpt_SetCellValue (worksheetHandle9, rangeA1, CAVT_CSTRING , label1);
					end=0;
				}


			}
			end = 1;
		}
		//end = 1;

		NBTestCheck++;
		return 0;
	}
	else
	{

		return -1;
	}


}

//***********************************************************************************************
// Modif - Carolina 04/2019
// find time that correspond to the label and insert one label into Logs.xlsx
//***********************************************************************************************
int InsertOneLabel()
{
	int boucle = 0, end = 1;
	char TimeColumn[255] = "";
	char range[30] = "";
	char rangeA[30] = "";
	char rangeA1[30] = "";
	char searchData[15] = "/ROW/@_RTime";
	char foundCellRangeTime[50]="";
	float intTimeColumn = 0;
	char cellTime[255] = "";
	char AddLabel1_condition[255] = "";
	
	if (strcmp(label1," ;") !=0)
	{
//	ExcelRpt_ApplicationNew(VFALSE,&applicationHandle9); //needs to be VFALSE
	ExcelRpt_WorkbookOpen (applicationHandleProject, fichierLogxml, &workbookHandle9);
	ExcelRpt_GetWorksheetFromIndex(workbookHandle9, 1, &worksheetHandle9);

	//printf("i am in InsertOneLabel\n\n\n");
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, searchData, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,foundCellRangeTime);
	char caracter1[] = "$";
	char *token1 = strtok(foundCellRangeTime, caracter1);
	if(token1 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", searchData);
	else
		strcpy(cellTime, token1);

	if(token1 != NULL)
	{
		if(Time1 !=0)// && (Time2 == 0))
		{
			//parcourir column to find time
			for(boucle=2; end==1; boucle++)
			{
				sprintf(range,"%s%d",cellTime, boucle+1);
				ExcelRpt_GetCellValue (worksheetHandle9, range, CAVT_CSTRING, TimeColumn);
				intTimeColumn = atof(TimeColumn);

				if(intTimeColumn == Time1)
				{
					sprintf(rangeA1, "A%d", boucle);
					ExcelRpt_GetCellValue (worksheetHandle9, rangeA1, CAVT_CSTRING, AddLabel1_condition);

					if(strcmp(AddLabel1_condition, label1) != 0)
					{
						ExcelRpt_InsertRow(worksheetHandle9, boucle+1, 1);
						sprintf(rangeA, "A%d", boucle+1);
						ExcelRpt_SetCellValue (worksheetHandle9, rangeA, CAVT_CSTRING , label1);
						boucle=boucle+1;
						end = 0;
					}
					else
					{
						end=0;
					}
				}

			}
			end=1;
		}


		NBTestCheck++;
		return 0;
	}
	else
	{

		return -1;
	}
	}
	return 0;
}

//MARVYN 05/07/2021******************************************************************************

int InsertOneLabel2()
{
	int boucle = 0, end = 1;
	char TimeColumn[255] = "";
	char range[30] = "";
	char rangeA[30] = "";
	char rangeA1[30] = "";
	char searchData[15] = "/ROW/@_RTime";
	char foundCellRangeTime[50]="";
	float intTimeColumn = 0;
	char cellTime[255] = "";
	char AddLabel2_condition[255] = "";
	
	/*if (Time1 == 0)
	{
		Time1=1000;
		fprintf(file_handle, "Problem into InsertTwoLabel() function : time of Label1 = 0.Set by default at 1000 ms.\n");
	 }*/
	
//	ExcelRpt_ApplicationNew(VFALSE,&applicationHandle9); //needs to be VFALSE
	ExcelRpt_WorkbookOpen (applicationHandleProject, fichierLogxml, &workbookHandle9);
	ExcelRpt_GetWorksheetFromIndex(workbookHandle9, 1, &worksheetHandle9);

	//printf("i am in InsertOneLabel2\n\n\n");
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, searchData, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,foundCellRangeTime);
	char caracter1[] = "$";
	char *token1 = strtok(foundCellRangeTime, caracter1);
	if(token1 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", searchData);
	else
		strcpy(cellTime, token1);

	if(token1 != NULL)
	{
		if(Time2 !=0)// && (Time2 != 0))
		{
			//parcourir column to find time
			for(boucle=2; end==1; boucle++)
			{
				sprintf(range,"%s%d",cellTime, boucle+1);
				ExcelRpt_GetCellValue (worksheetHandle9, range, CAVT_CSTRING, TimeColumn);
				intTimeColumn = atof(TimeColumn);

				if(intTimeColumn == Time2)
				{
					sprintf(rangeA1, "A%d", boucle);
					ExcelRpt_GetCellValue (worksheetHandle9, rangeA1, CAVT_CSTRING, AddLabel2_condition);

					if(strcmp(AddLabel2_condition, label2) != 0)
					{
						ExcelRpt_InsertRow(worksheetHandle9, boucle+1, 1);
						sprintf(rangeA, "A%d", boucle+1);
						ExcelRpt_SetCellValue (worksheetHandle9, rangeA, CAVT_CSTRING , label2);
						boucle=boucle+1;
						end = 0;
					}
					else
					{
						end=0;
					}
				}

			}
			end=1;
		}


		NBTestCheck++;
		return 0;
	}
	else
	{

		return -1;
	}

}





//***********************************************************************************************
// Modif - Carolina 04/2019
// Translate parameter from database
//***********************************************************************************************
double traslateParameterValue(char *varTolerance)  //tolerence = value;
{
	//Point cell;
	//Rect  rect;
	//BOOL bValid;
	BOOL bParam = FALSE;
	int a=0;
	const char s[2] = " ";
	char *token;
	char *tabtoken[200] = {NULL};
	char range[30];
	char ParameterName[100];
	char unitParameter[10] ="";
	char Value[100];
	char *TabValue[100] = {NULL};
	char *parameterCompare=NULL;
	int boucle;
	int end = 1;
	int StringEqual=0;
	int findend = 0;
	int stringFound = 0;
	char Value1[255]= "";
	double iValue1 = 0.0;
	char milisec[6]="[ms]";
	char percent[6] = "[%]";
	char number[10] = "[number]";
	char auxValue1[100]="";
	char defaultDirectory[MAX_PATHNAME_LEN] = "D:\\CalculationFile.xlsx";


	//ExcelRpt_ApplicationNew(VFALSE,&applicationHandle);
	//ExcelRpt_WorkbookOpen (applicationHandleProject, GsCheckCfgFile, &workbookHandledata);
	//ExcelRpt_GetWorksheetFromIndex(workbookHandledata, 2, &worksheetHandledata2);

	strtok(varTolerance, ";");

	strncpy (ChaineExcel, "=", 200);
	token = strtok(varTolerance, s);
	tabtoken[a]=token;
	if(token == NULL)
	{
		token = "0";
	}

	for(boucle=1; end==1; boucle++) //take from database
	{
		sprintf(range,"A%d",boucle+1); //A2
		ExcelRpt_GetCellValue (worksheetHandledata2, range, CAVT_CSTRING, ParameterName);
		StringEqual=strcmp(ParameterName, token); //erreur si time and duration sont vide

		if(StringEqual == 0)
		{
			sprintf(range,"B%d",boucle+1);
			ExcelRpt_GetCellValue (worksheetHandledata2, range, CAVT_CSTRING, Value);
			sprintf(range,"C%d",boucle+1); //colonne C unités
			ExcelRpt_GetCellValue (worksheetHandledata2, range, CAVT_CSTRING, unitParameter);
			TabValue[a]=Value;
			strcat(ChaineExcel, TabValue[a]);
			bParam = TRUE;
			stringFound = 1;
		}
		// cherche la fin de la liste
		parameterCompare=strchr(ParameterName,'end');
		if(parameterCompare != NULL)
		{
			end=0;
			findend =1;
		}
		if(findend ==1 && stringFound == 0)
		{

			strcpy(auxValue1, varTolerance);
			iValue1 = atof(auxValue1);
			return iValue1;  //Ex return tolerence = 0.0000
		}
	}
	if (bParam==FALSE)
	{
		strcat(ChaineExcel, tabtoken[a]);
	}
	a++;

	/* walk through other tokens */
	while( token != NULL )
	{
		token = strtok(NULL, s);
		tabtoken[a]=token;
		end=1;
		bParam = FALSE;

		for(boucle=1; end==1; boucle++)
		{
			sprintf(range,"A%d",boucle+1);
			ExcelRpt_GetCellValue (worksheetHandledata2, range, CAVT_CSTRING, ParameterName);

			if(token!=NULL)
			{
				StringEqual=strcmp(ParameterName, token);
			}
			if(StringEqual == 0)
			{
				sprintf(range,"B%d",boucle+1);
				ExcelRpt_GetCellValue (worksheetHandledata2, range, CAVT_CSTRING, Value);
				TabValue[a]=Value;
				strcat(ChaineExcel, TabValue[a]);
				bParam = TRUE;
			}
			parameterCompare=strchr(ParameterName,'end');
			if(parameterCompare != NULL)
			{
				end=0;
			}
		}
		if ((bParam==FALSE)&&(token!=NULL) )
		{
			strcat(ChaineExcel, tabtoken[a]);
		}
		//	if( token!=NULL)
		//	{}
		a++;
	}

	//ExcelRpt_ApplicationNew(VFALSE,&applicationHandle7);
	ExcelRpt_WorkbookOpen (applicationHandleProject,defaultDirectory, &workbookHandle1);
	ExcelRpt_GetWorksheetFromIndex(workbookHandle1, 1, &worksheetHandle1);
	ExcelRpt_SetCellValue (worksheetHandle1, "A1", CAVT_CSTRING, ChaineExcel);
	ExcelRpt_GetCellValue (worksheetHandle1, "A1", CAVT_CSTRING, &Value1);


	strcpy(auxValue1, Value1);  //Value1 = String
	//converts the Value on seconds
	if((strcmp(unitParameter, milisec)) == 0)
	{
		iValue1 = atof(auxValue1);
		iValue1 = iValue1/1000;
		return iValue1;
	}
	if((strcmp(unitParameter, percent)) == 0)
	{
		iValue1 = atof(auxValue1);
		iValue1 = iValue1;
		return iValue1;
	}
	if((strcmp(unitParameter, number)) == 0)
	{
		iValue1 = atof(auxValue1);
		iValue1 = iValue1;
		return iValue1;
	}
	else
	{
		iValue1 = atof(auxValue1);
		return iValue1;
	}

//	return Value1;

}

//***********************************************************************************************
// Modif - Carolina 07/2019
//
//***********************************************************************************************
int CheckTimingInterFrames(char * wuID, char *realValue, char *FCParam, char *FunctionCode, double dtolValue, double dtolpercent, char* InterfacetoCheck)
{
	int boucle = 0, end = 1, endCondition = 1, zerofound = 0, CheckTIF=0, nb = 0, n=0, m=0, l=0, valide = 0;
	char CheckGoodsum[15] = 	"/ROW/@";
	char CheckRTime[15] = 		"/ROW/@";
	char CKSGood[3] = "";
	char _ATimeColumn[255] = "";
	char _RTimeColumn[255] = "";
	char rangeA[30] = "";
	char rangeTime[30]="";
	char GoodSumRange[30] = "";
	char foundCellGoodSumRange[30] = "";
	char cellTimeRange[30]="";
	char cellGoodSumRange[30] = "";
	char Rowparam2[255] = 		"/ROW/@";  //for FC
	char FCRow[30] = "";
	char RTime[30] = "";
	char cellFCRange[30] = "";
	char *CheckParamFC;
	char rangeFC[30] = "";
	char _FunctionCode[50] = "";
	int countNbBurst = 0, counNbofFrames = 0;
	double dtime = 0.0, auxtime = 0.0, sub = 0.0,
		   dValue = 0.0, dValueMax=0.0, dValueMin = 0.0, dValueMaxperc = 0.0, dValueMinperc = 0.0;
	//auxdVal = 0.0; //dValMax=0.0, dValMin = 0.0, dValueMaxpercSeq=0.0, dValueMinpercSeq = 0.0;
	char *tabVal[20] = {NULL};
	double dVal[20]= {0}, dValMax[20]= {0}, dValMin[20]= {0}, dValueMaxpercSeq[20]= {0}, dValueMinpercSeq[20]= {0};
	char auxVal[255] = "";
	double auxdValMax = 0.0;
	double auxdValMin = 0.0;
	double auxdValueMaxpercSeq = 0.0;
	double auxdValueMinpercSeq = 0.0;
	int notvalide = 0;

	int CKSGoodvalid = 0;
	int CKSGoodinvalid = 0;

	char CheckWUID[30] = 		"/ROW/@";
	char _WUIDColumn[255] = "";
	char WUIDRange[30] = "";
	char cellWUIDRange[30] = "";
	char WUID[30] = "";
	int noLab=0; 

	char CheckDesc[15] = 	"/ROW/@Desc";
	char RFProt[30]="";
	char cellRFProtocol[30]="";
	char RFprotocol[255]="";
	char RFrange[30]="";
	

	strcat(CheckGoodsum,translateData(InterfacetoCheck, "CKS_Validity"));
	strcat(CheckWUID,translateData(InterfacetoCheck,"ID"));
	strcat(CheckRTime,translateData(InterfacetoCheck, "Time"));
	//printf("CheckWUID = %s\n",CheckWUID);
//printf("CheckRTime = %s\n",CheckRTime);
	//printf("CheckGoodsum = %s\n",CheckGoodsum); 

	
	
	strcpy(auxVal, realValue); 						 // auxVal = "105;" or "105;117;129;" (fixed value or a sequence)
	nb = conta(auxVal);

	if(nb == 1)    									// Ex: "Interfrm1" -> 105 [ms] or  "0.105;"
	{
		dValue = atof(auxVal); 	//Ex:   dValue have to be 0.105
		dValue = dValue*1000; 						// dValue = 105
	}
	// sequence value
	if(nb > 1)  									//105;117;129; Ex: nb = 3
	{
		char *token = strtok(auxVal, ";");
		while( token != NULL )
		{
			tabVal[n]=token;
			token = strtok(NULL, ";");
			n++;
		}
		for(int k=0; k<nb; k++)   //0;1;2 nb=3
		{
			dVal[k] = atof(tabVal[k]);
			dVal[k] = (dVal[k])*1000;  
		}
	}

	dtolValue = dtolValue*1000; //Modif MaximePAGES 03/07/2020 - on nous donne une tol *1000, on le /1000 car l'user doit donner en ms déjà.

	char percent[4] = "[%]";
	fprintf(file_handle, "\n***************CheckTimingInterFrames***************\n");
	fprintf(file_handle, "                 Check_%d\n\n", NBTestCheck);


	CheckParamFC = strcat(Rowparam2, FCParam); //EX: /ROW/@Function_code
	//Worksheet Logs 
	ExcelRpt_WorkbookOpen (applicationHandleProject, fichierLogxml, &workbookHandle9); 
	ExcelRpt_GetWorksheetFromIndex(workbookHandle9, 1, &worksheetHandle9);

	
	//3 - looking for "/ROW/@Desc" 
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckDesc, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,RFProt);
	char caracterDesc[] = "$";
	char *tokenDesc = strtok(RFProt, caracterDesc);
	if(tokenDesc == NULL)
	{
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckDesc);
		return 1;
	}
	 else
	   strcpy(cellRFProtocol, tokenDesc); 
	
	
	// 1 - looking for "/ROW/@CKSGood"
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckGoodsum, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,foundCellGoodSumRange);
	char caracter1[] = "$";  //foundCellRange =  $O$2
	char *token1 = strtok(foundCellGoodSumRange, caracter1);   //token1 = O
	if(token1 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckGoodsum);
	else
		strcpy(cellGoodSumRange, token1);

	//2 - looking for "/ROW/@Function_code" = $W$2
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckParamFC, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,FCRow);
	char caracter2[] = "$";
	char *token2 = strtok(FCRow, caracter2);
	if(token2 == NULL)
		fprintf(file_handle, "The column %s was not found, RFprotocole \"%s\" not received!\n\n", CheckParamFC,InterfacetoCheck);
	else
		strcpy(cellFCRange, token2); // cellFCRange = W2

	//3 - looking for "/ROW/@_RTime" = $B$2
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckRTime, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,RTime);
	char caracter3[] = "$";
	char *token3 = strtok(RTime, caracter3);
	if(token3 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckRTime);
	else
		strcpy(cellTimeRange, token3); // cellFCRange = B2
	
	//4 - looking for "/ROW/@ID" = $W$2 // the WU ID
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckWUID, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,WUID);
	char caracterWUID[] = "$";
	char *tokenWUID = strtok(WUID, caracterWUID);
	if(tokenWUID == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckWUID);
	else
		strcpy(cellWUIDRange, tokenWUID);


	if(token1 != NULL && token2 != NULL && token3 != NULL && tokenWUID != NULL)
	{
		if (lab2 == 0)
		{
			if (lab1 == 0)
			{
			fprintf(file_handle, "Between %s (%d ms) and %s (%d ms)\n", 	label1, Time1, label2, Time2);
			}
			else
			{
			fprintf(file_handle, "Between the beginning and %s (%d ms)\n", label2, Time2);
			}
		}
		else
		{
		   	if (lab1 == 0)
			{
			fprintf(file_handle, "Between %s (%d ms) and the end of the script\n", 	label1, Time1);
			}
			else
			{
			fprintf(file_handle, "Between the beginning and the end of the script\n");
			}
		}
		fprintf(file_handle, "Function code: %s\n",		FunctionCode);
		fprintf(file_handle, "Expected IF timing: %s sec\n",				realValue);
		fprintf(file_handle, "Tolerance value: %f ms\n", 	dtolValue);
		fprintf(file_handle, "Tolerance %s: %f\n\n",	percent, dtolpercent);


		for(boucle=2; end==1; boucle++)
		{
			//until find label1
			sprintf(rangeA, "A%d", boucle+1);
			ExcelRpt_GetCellValue (worksheetHandle9, rangeA, CAVT_CSTRING, _ATimeColumn);

			//starting check when label1 was found
			if(strcmp(label1, _ATimeColumn) == 0 || lab1==1)
			{
				//analyse until find label 2
				for(int i = boucle+1 ; endCondition == 1; i++)
				{
					sprintf(RFrange, "%s%d", cellRFProtocol, i);
					ExcelRpt_GetCellValue (worksheetHandle9, RFrange, CAVT_CSTRING,RFprotocol);
				    
					if (strcmp(InterfacetoCheck,"") == 0 || CheckRFProtocol(InterfacetoCheck,RFprotocol) == 1 )
					{
					//1 - check Goodsm first
					sprintf(GoodSumRange,"%s%d",cellGoodSumRange, i); //P3
					ExcelRpt_GetCellValue (worksheetHandle9, GoodSumRange, CAVT_CSTRING, CKSGood);

					if(strcmp(CKSGood, "1") == 0)
					{

						// we look for the good WU ID
						sprintf(WUIDRange,"%s%d",cellWUIDRange, i);
						ExcelRpt_GetCellValue (worksheetHandle9, WUIDRange, CAVT_CSTRING, _WUIDColumn);

						if(strcmp(_WUIDColumn, wuID) == 0)
						{

							CKSGoodvalid++;

							//2 - check FC second
							sprintf(rangeFC,"%s%d",cellFCRange, i);
							ExcelRpt_GetCellValue (worksheetHandle9, rangeFC, CAVT_CSTRING, _FunctionCode);

							if(strcmp(_FunctionCode, FunctionCode) == 0)
							{
								//check nb bursts
								//2 - check _RTime
								sprintf(rangeTime,"%s%d",cellTimeRange, i);
								ExcelRpt_GetCellValue (worksheetHandle9, rangeTime, CAVT_CSTRING, _RTimeColumn);
								dtime = atof(_RTimeColumn);

								if(nb == 1) //one value
								{
									if(auxtime == 0)  //first emission
									{
										sub =  dtime -  auxtime;
										auxtime = dtime;

										countNbBurst++;
										counNbofFrames++;
										//fprintf(file_handle, "\n%d\n\n", countInterBurst);
									}
									else
									{
										sub =  dtime -  auxtime;
										auxtime = dtime;
										//fprintf(file_handle, "\nsub %f\n", sub);
										counNbofFrames++;
										if(sub < IFandIBthreshold)  //IB > 2s
										{
											//fprintf(file_handle, "\nTimingInterFrame: %f\n", sub);
											//counNbofFrames++;
											if(dtolValue != 0.0 && dtolpercent == 0.0)
											{
												//Tolerance 									//tolValue
												dValueMax = dValue + dtolValue;
												dValueMin = dValue - dtolValue;
												if(dValueMin<0)
													dValueMin = 0;

												if(sub >= dValueMin && sub <= dValueMax )
												{
													//counNbofFrames++;
													valide++;
													//fprintf(file_handle, "The Timing inter Frame %f is between [%f,%f]\n", sub, dValueMin, dValueMax);
												}
												else
												{
													notvalide++;
													CheckTIF = 1;
													fprintf(file_handle, "IF timing %f is not between [%f,%f], (frames at %f and %f)\n", sub, dValueMin, dValueMax,(dtime-sub),dtime);
												}
											}
											else if(dtolValue == 0.0 && dtolpercent != 0.0)
											{
												//Tolerance %									//tolValue
												dValueMaxperc = dValue + (dValue*(dtolpercent));
												dValueMinperc = dValue - (dValue*(dtolpercent));
												if(dValueMinperc<0)
													dValueMinperc = 0;

												if(sub >= dValueMinperc && sub <= dValueMaxperc )
												{
													//counNbofFrames++;
													//fprintf(file_handle, "\nsub %f\n", sub);
													valide++;
													//fprintf(file_handle, "The Timing inter Frame %f is between [%f,%f]\n", sub, dValueMinperc, dValueMaxperc);
												}
												else
												{
													CheckTIF = 1;
													notvalide++;
													fprintf(file_handle, "IF timing %f is not between [%f,%f], (frames at %f and %f)\n", sub, dValueMinperc, dValueMaxperc,(dtime-sub),dtime);
												}

											}
											else 
											{
				
												if(dValue<0)
													dValue = 0;

												if(sub >= dValue && sub <= dValue )
												{
													//counNbofFrames++;
													//fprintf(file_handle, "\nsub %f\n", sub);
													valide++;
													//fprintf(file_handle, "The Timing inter Frame %f is between [%f,%f]\n", sub, dValueMinperc, dValueMaxperc);
												}
												else
												{
													CheckTIF = 1;
													notvalide++;
													fprintf(file_handle, "IF timing %f is not equal to [%f], (frames at %f and %f)\n", sub, dValue,(dtime-sub),dtime);
												}
											}	
										}
										else countNbBurst++;
									}
								}
								if(nb > 1)
								{
									if(auxtime == 0)  //first emission
									{
										sub =  dtime -  auxtime;
										auxtime = dtime;
										//fprintf(file_handle, "\nsub %f\n", sub);
										countNbBurst++;
										counNbofFrames++;
									}
									else
									{
										sub =  dtime -  auxtime;
										auxtime = dtime;
										notvalide = 0;
										counNbofFrames++;
										if(sub < IFandIBthreshold)  //IB > 1s
										{
											//fprintf(file_handle, "\nTimingInterFrame: %f\n", sub);
											//counNbofFrames++;

											if(dtolValue != 0.0 && dtolpercent == 0.0)
											{
												for(m = 0; m<nb; m++)
												{
													dValMax[m] = dVal[m] + dtolValue;  //m vectors
													dValMin[m] = dVal[m] - dtolValue;  //m vectors
													//m++;
												}
												//compare values
												for(l=0; l<nb; l++)
												{
													auxdValMax = dValMax[l];
													auxdValMin = dValMin[l];
													if(auxdValMin<0)
														auxdValMin = 0;

													if(sub >= auxdValMin  && sub <= auxdValMax)
													{
														//fprintf(file_handle, "The Timing inter Frame %f is between [%f,%f]\n", sub, auxdValMin, auxdValMax);
														valide++;
													}
													else
													{
														notvalide++;
														fprintf(file_handle, "IF timing %f is not between [%f,%f], (frames at %f and %f)\n", sub, auxdValMin, auxdValMax,(dtime-sub),dtime);
													}
												}
											}
											else if(dtolValue == 0.0 && dtolpercent != 0.0)
											{
												for(m = 0; m<nb; m++)
												{
													dValueMaxpercSeq[m] = dVal[m] + ((dVal[m])*(dtolpercent));  //m vectors
													dValueMinpercSeq[m] = dVal[m] - ((dVal[m])*(dtolpercent));  //m vectors
												}

												//compare values
												for(l=0; l<nb; l++)
												{
													auxdValueMaxpercSeq = dValueMaxpercSeq[l];
													auxdValueMinpercSeq = dValueMinpercSeq[l];
													if(auxdValueMinpercSeq<0)
														auxdValueMinpercSeq = 0;

													if(sub >= dValueMinpercSeq[l] && sub <= dValueMaxpercSeq[l])
													{
														//fprintf(file_handle, "The Timing inter Frame %f is between [%f,%f]\n", sub, auxdValueMinpercSeq, auxdValueMaxpercSeq);
														valide++;
													}
													else
													{
														notvalide++;
														fprintf(file_handle, "IF timing  %f is not between [%f,%f], (frames at %f and %f)\n", sub, auxdValueMinpercSeq, auxdValueMaxpercSeq,(dtime-sub),dtime);
													}
												}
											}
											else 
											{
												//compare values
												for(l=0; l<nb; l++)
												{
													auxdValueMaxpercSeq = dVal[l];
													auxdValueMinpercSeq = dVal[l];
													if(dVal<0)
														strcpy(dVal,0);

													if(sub >= dVal[l] && sub <= dVal[l])
													{
														//fprintf(file_handle, "The Timing inter Frame %f is between [%f,%f]\n", sub, auxdValueMinpercSeq, auxdValueMaxpercSeq);
														valide++;
													}
													else
													{
														notvalide++;
														fprintf(file_handle, "IF timing  %f is not equal to [%f], (frames at %f and %f)\n", sub, dVal,(dtime-sub),dtime);
													}
												}	
											}
										}
										else countNbBurst++;
									}

									if(notvalide == nb)
									{
										notvalide=0;
										CheckTIF = 1;
									}

								}
							}
							else fprintf(file_handle, "Invalid Function Code (%s)  found in line %d\n",_FunctionCode, i);


						}

					}
					if(strcmp(CKSGood, "0") == 0)
					{
						zerofound = 1;
						CKSGoodinvalid++;
						fprintf(file_handle, "CKSGood: 0 was found in line %d\n", i);
					}


					sprintf(rangeA, "A%d", i);
					ExcelRpt_GetCellValue (worksheetHandle9, rangeA, CAVT_CSTRING, _ATimeColumn);

					if(strcmp(label2, _ATimeColumn) == 0)   //stop condition of check
					{
						if (noLab == 1 || FirstLab==1  || TwoLabs==1)
						{
						if(zerofound == 1)
						{
							float percentageCKSGood = ((double)CKSGoodinvalid/( (double)CKSGoodinvalid+ (double)CKSGoodvalid))*100.0;
							fprintf(file_handle, "\n%.2f%% of the CKSGood are equal to 0!\n\n",percentageCKSGood );
						}


						endCondition = 0; //stop ckeck
						end = 0;
						}
						noLab=1;
					}   
				}
				}
			}
		}
		end=1;


		if(notvalide > 0) //Modif MaximePAGES 07/07/2020
		{
			CheckTIF = 1;
		}
		if(counNbofFrames == 0)
		{
			CheckTIF = 1;
		}


		//return CheckTIF;
	}
	else
	{
		fprintf(file_handle, "The CheckTimingInterFrames was not performed!\n\n");
		CheckTIF = -1;
		//return CheckTIF;
	}

	printCheckResults (file_handle, "CheckTimingInterFrames", CheckTIF, valide, notvalide, counNbofFrames, countNbBurst, 0,0.0,0.0,0.0);
	return CheckTIF;

}

//***********************************************************************************************
// Modif - Carolina 04/2019
//
//***********************************************************************************************
int CheckFieldValue(char * wuID, char *Parametertocheck, char *Value, double tolValue, double tolpercent, char* InterfacetoCheck)
{
	int boucle = 0, end = 1, failedParam = 0, varCheckFieldValue = 0, endCondition = 1, zerofound = 0, nb=0, n=0;
	double iValue = 0.0, PosLimit = 0.0, NegLimit = 0.0, ivarParam = 0.0, dValue = 0.0; //, auxdVal=0.0;
	char *CheckParam;
	char _ATimeColumn[255] = "";
	char range[30] = "";
	char rangeA[30] = "";
	char foundCellRange[30] = "";
	char cellParamRange[30] = "";
	char varParam[100]="";
	char Rowparam[255] = "/ROW/@";
	char CKSGood[3] = "";
	char GoodSumRange[30] = "";
	char foundCellGoodSumRange[30] = "";
	char cellGoodSumRange[30] = "";
	char CheckGoodsum[100] = "/ROW/@";
	double dRange1 = 0.0, dRange2=0.0;
	char auxVal[255] = "";
	char valueForPrint[255] = "";
	char auxVal2[255]="";
	char *tabVal[20] = {NULL};
	int typeValue = 0 , notvalide = 0, valide=0, notvalidframes = 0, validframes = 0;
	char percent[4] = "[%]";
	//double dRange1Max = 0.0;
	//double dRange1Min = 0.0;
	//double dRange2Max = 0.0;
	//double dRange2Min = 0.0;
	double dVal[20]= {0}, dValMax[20]= {0}, dValMin[20]= {0}; // dValueMaxpercSeq[20], dValueMinpercSeq[20];
	double auxdValMax = 0.0;
	double auxdValMin = 0.0;
	//double auxdValueMaxpercSeq = 0.0;
	//double auxdValueMinpercSeq = 0.0;

	int CKSGoodvalid = 0;
	int CKSGoodinvalid = 0;

	char CheckWUID[30] = 		"/ROW/@";
	char _WUIDColumn[255] = "";
	char WUIDRange[30] = "";
	char cellWUIDRange[30] = "";
	char WUID[30] = "";
	int noLab =0;
	
	char CheckDesc[15] = 	"/ROW/@Desc";
	char RFProt[30]="";
	char cellRFProtocol[30]="";
	char RFprotocol[255]="";
	char RFrange[30]="";
	
	
	strcat(CheckGoodsum,translateData(InterfacetoCheck, "CKS_Validity"));
	strcat(CheckWUID,translateData(InterfacetoCheck,"ID"));
	//printf("CheckWUID = %s\n",CheckWUID);
	//printf("CheckGoodsum = %s\n",CheckGoodsum); 

	

	strcpy(valueForPrint,Value);
	strcpy(auxVal, Value);  // auxVal = "0-4;" or "105;" or "105;117;129;"
	nb = conta(auxVal);

	//determinates if value is a fixed value, list of values, or a range
	if(nb == 1)    // 0-4; or 105;
	{

		char *token2 = strstr(auxVal, ":");
		if(token2 != NULL) //there is a - then range
		{
			RetrieveandSeparateRange(Value); // 0-4;
			dRange1 = atof(range1);
			dRange2 = atof(range2);
			typeValue = 0;
		}
		else  //NULL pointer if the character does not occur in the string
		{
			//one value
			dValue = atof(auxVal);
			dValue = dValue;
			typeValue = 1;
		}
	}
	// list of values
	if(nb > 1)  //105;117;129; Ex: nb = 3
	{
		char *token = strtok(auxVal, ";");
		while( token != NULL )
		{
			tabVal[n]=token;
			token = strtok(NULL, ";");
			n++;
		}
		for(int k=0; k<nb; k++)   //0;1;2 nb=3
		{
			dVal[k] = atof(tabVal[k]);
			dVal[k] = (dVal[k]);
			//auxdVal = dVal[k];
			//fprintf(file_handle, "\ndVal[%d] = %f\n", k, auxdVal);
		}
		typeValue = 2;
	}

	//head of the check
	fprintf(file_handle, "\n**************CheckFieldValue**************\n");
	fprintf(file_handle, "                 Check_%d\n\n", NBTestCheck);

	//check param
	CheckParam = strcat(Rowparam, Parametertocheck);  //Ex CheckParam = /ROW/@Channel
	//take excel file
	//ExcelRpt_ApplicationNew(VFALSE,&applicationHandle9);
	//ExcelRpt_WorkbookOpen (applicationHandle9, fichierLogxlsx, &workbookHandle9);
	ExcelRpt_GetWorksheetFromIndex(workbookHandle9, 1, &worksheetHandle9);  //Worksheet Configuration

	//3 - looking for "/ROW/@Desc" 
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckDesc, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,RFProt);
	char caracterDesc[] = "$";
	char *tokenDesc = strtok(RFProt, caracterDesc);
	if(tokenDesc == NULL)
	{
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckDesc);
		return 1;
	}
	 else
	   strcpy(cellRFProtocol, tokenDesc);
	
	//looking for "/ROW/@CKSGood"
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckGoodsum, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,foundCellGoodSumRange);
	char caracter1[] = "$";  //foundCellRange =  $P$2
	char *token1 = strtok(foundCellGoodSumRange, caracter1);   //token1 = C
	if(token1 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckGoodsum);
	else
		strcpy(cellGoodSumRange, token1);

	//looking for "/ROW/@parameter"
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckParam, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,foundCellRange);
	//EX: foundCellRange =  $M$2
	char caracter[] = "$";
	char *token3 = strtok(foundCellRange, caracter);   //token1 = C
	if(token3 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckParam);
	else
		strcpy(cellParamRange, token3);
	
	// - looking for "/ROW/@ID" = $W$2 // the WU ID
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckWUID, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,WUID);
	char caracterWUID[] = "$";
	char *tokenWUID = strtok(WUID, caracterWUID);
	if(tokenWUID == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckWUID);
	else
		strcpy(cellWUIDRange, tokenWUID);


	if(token1 != NULL && token3 != NULL && tokenWUID != NULL)
	{
		if (lab2 == 0)
		{
			if (lab1 == 0)
			{
			fprintf(file_handle, "Between %s (%d ms) and %s (%d ms)\n", 	label1, Time1, label2, Time2);
			}
			else
			{
			fprintf(file_handle, "Between the beginning and %s (%d ms)\n", label2, Time2);
			}
		}
		else
		{
		   	if (lab1 == 0)
			{
			fprintf(file_handle, "Between %s (%d ms) and the end of the script\n", 	label1, Time1);
			}
			else
			{
			fprintf(file_handle, "Between the beginning and the end of the script\n");
			}
		}
		fprintf(file_handle, "Parameter: %s\n", 		CheckParam);
		fprintf(file_handle, "Expected %s value(s): %s\n", Parametertocheck,	valueForPrint);
		fprintf(file_handle, "Tolerance value: %f\n", 	tolValue);
		fprintf(file_handle, "Tolerance %s: %f\n\n",	percent, tolpercent);

		strcpy(auxVal2, Value);
		strtok(auxVal2 , ";");

		//vefirifier  entre labels
		//parcourir column to find time
		for(boucle=2; end==1; boucle++)
		{
			//until find label1

			sprintf(rangeA, "A%d", boucle+1);
			ExcelRpt_GetCellValue (worksheetHandle9, rangeA, CAVT_CSTRING, _ATimeColumn);

//			if(strcmp(label1, "StartTest") == 0) 
			//|| label1 == NULL or strcmp(label1,"") ==0
			if(strcmp(label1, _ATimeColumn) == 0 || lab1==1)
			{
				//printf("label1 = %s and ATIME = %s",label1,_ATimeColumn);
				//printf("je suis bien entré");
				//analyse until find label 2
				for(int i = boucle+1 ; endCondition == 1; i++)
				{
					sprintf(RFrange, "%s%d", cellRFProtocol, i);
					ExcelRpt_GetCellValue (worksheetHandle9, RFrange, CAVT_CSTRING,RFprotocol);
				    
					if (strcmp(InterfacetoCheck,"") == 0 || CheckRFProtocol(InterfacetoCheck,RFprotocol) == 1 )
					{
					
					//check Goodsm first
				
					sprintf(GoodSumRange,"%s%d",cellGoodSumRange, i);//P3
					ExcelRpt_GetCellValue (worksheetHandle9, GoodSumRange, CAVT_CSTRING, CKSGood);
					//printf("CKSGood = %s\n",CKSGood);
					if(strcmp(CKSGood, "1") == 0)
					{
						
						sprintf(WUIDRange,"%s%d",cellWUIDRange, i);//P3
					
						
						ExcelRpt_GetCellValue (worksheetHandle9, WUIDRange, CAVT_CSTRING, _WUIDColumn);

						if(strcmp(_WUIDColumn, wuID) == 0)
						{
						  //printf("OK2"); 
							CKSGoodvalid++ ;
							//***************************************************************************
							//case where you are looking for an exact value
							//introduce tolerance  if 0 -> these tolerances correspond to the exact value
							if( tolpercent == 0 && tolValue == 0)
							{
								//start analysis
							
								sprintf(range,"%s%d",cellParamRange, i);
								ExcelRpt_GetCellValue (worksheetHandle9, range, CAVT_CSTRING, varParam);  //varParam = Value
								switch(typeValue)
								{
									case 0:   //range

										if(strcmp(varParam, "") != 0)
										{
											ivarParam = atof(varParam);

											//dRange1 = dRange1 - tolValue;
											if(dRange1<0)
												dRange1 = 0;
											//dRange2 = dRange2 + tolValue;
											if(ivarParam >= dRange1 && ivarParam <= dRange2)
											{
												//printf("***1***\n");
												validframes++;

											}
											else
											{
												notvalidframes++;
												fprintf(file_handle, "The received value - %s - in the line %d is not between the range [ %f, %f] \n", varParam, i, dRange1, dRange2);    //range
												failedParam = 1;
											}
										}
										break;



									case 1:   //one value
										//printf("\nvarParam = %s and  auxVal2 = %s\n",varParam,auxVal2);
										if((strcmp(varParam, "") != 0) && (strcmp(varParam, auxVal2) != 0))
										{
											notvalidframes++;
											fprintf(file_handle, "The expected value - %s - does not match to the received value - %s - in the line %d\n", Value, varParam, i);    //range
											failedParam = 1;
										}
										else validframes++;//printf("***10***\n");

										break;

									case 2:   //list of values


										if(strcmp(varParam, "") != 0)
										{

											for(int m = 0; m<nb; m++)
											{
												if (strcmp(varParam, tabVal[m]) != 0)
												{
													notvalide++;
												}
												else valide++;//,printf("***2***\n");

											}

											if(notvalide == nb)
											{
												notvalidframes++;
													//printf("***3***\n");    
												fprintf(file_handle, "The expected values - %s - do not match to the received value - %s - in the line %d\n", Value, varParam, i);    //range  											failedParam = 1;
												notvalide=0;
												failedParam = 1;
											}
											else
											{
												validframes++;
													//printf("***2***\n");    
												notvalide=0;
											}
										}

										break;
								}

							}

							//***************************************************************************
							//case where the tolerance is a limit but tolerance is not a percentage
							//introduce tolerance  "value : +/- value%
							//if((strcmp(Tolerancepercent, "0") == 0) && (strcmp(Tolerance, "0")) != 0)
							if(( tolpercent == 0) && ( tolValue != 0))
							{
								//iValue = atof(Value);

								//PosLimit = iValue + tolValue; //Ex: 6:+3
								//NegLimit = iValue - tolValue; //Ex: 6:-3

								sprintf(range,"%s%d",cellParamRange, i);
								ExcelRpt_GetCellValue (worksheetHandle9, range, CAVT_CSTRING, varParam);

								switch(typeValue)
								{
									case 0:   //range
										if(strcmp(varParam, "") != 0)
										{
											ivarParam = atof(varParam);

											dRange1 = dRange1 - tolValue;
											if(dRange1<0)
												dRange1 = 0;
											dRange2 = dRange2 + tolValue;
											if(ivarParam >= dRange1 && ivarParam <= dRange2)
											{
												validframes++;
												//printf("***4***\n");

											}
											else
											{
												notvalidframes++;
												fprintf(file_handle, "The received value - %s - in the line %d is not between the range [ %f, %f] \n", varParam, i, dRange1, dRange2);    //range
												failedParam = 1;
											}
										}
										break;

									case 1:   //one value
										if(strcmp(varParam, "") != 0)
										{
											ivarParam = atof(varParam);
											iValue = atof(Value);
											PosLimit = iValue + tolValue; //Ex: 6:+3
											NegLimit = iValue - tolValue; //Ex: 6:-3
											if(NegLimit<0)
												NegLimit = 0;

											if(ivarParam >= NegLimit && ivarParam <= PosLimit)
											{
												validframes++;
												//printf("***5***\n");

											}
											else
											{
												notvalidframes++;
												fprintf(file_handle, "The expected value - %s : +/- %f - does not match to the received value - %s - in the line %d\n", Value, tolValue, varParam, i);    //range
												failedParam = 1;
											}
										}
										break;

									case 2:   //list of values
										if(strcmp(varParam, "") != 0)
										{
											ivarParam = atof(varParam);

											for(int m = 0; m<nb; m++)
											{
												dValMax[m] = dVal[m] + tolValue;
												dValMin[m] = dVal[m] - tolValue;
												//m++;
											}
											//compare values
											for(int l=0; l<nb; l++)
											{
												auxdValMax = dValMax[l];
												auxdValMin = dValMin[l];
												if(auxdValMin<0)
													auxdValMin = 0;

												if(ivarParam >= auxdValMin  && ivarParam <= auxdValMax)
												{
													//do nothing - limit OK
													valide++;
												}
												else
													notvalide++;
											}

										}
										if(notvalide == nb)
										{
											notvalidframes++;
											fprintf(file_handle, "The expected values - %s : +/- %f - do not match to the received value - %s - in the line %d\n", Value, tolValue, varParam, i);    //range
											failedParam = 1;
											notvalide=0;
										}
										else
										{
											validframes++;
											//printf("***6***\n");
											notvalide=0;
										}
										break;
								}


							}
							
							if((tolpercent != 0) && ( tolValue == 0))
							{
								iValue = atof(Value);

								//PosLimit = iValue + tolpercent; //Ex: 6:+3
								//NegLimit = iValue - tolpercent; //Ex: 6:-3

								sprintf(range,"%s%d",cellParamRange, i);
								ExcelRpt_GetCellValue (worksheetHandle9, range, CAVT_CSTRING, varParam);

								switch(typeValue)
								{
									case 0:   //range
										if(strcmp(varParam, "") != 0)
										{

											ivarParam = atof(varParam);

											dRange1 = dRange1 - (dRange1*tolpercent);
											if(dRange1<0) dRange1 = 0;
											
											dRange2 = dRange2 + (dRange2*tolpercent);
											if(ivarParam >= dRange1 && ivarParam <= dRange2)
											{
												validframes++;
												//printf("***7***\n");

											}
											else
											{
												notvalidframes++;
												fprintf(file_handle, "The received value - %s - in the line %d is not between the range [ %f, %f] \n", varParam, i, dRange1, dRange2);    //range
												failedParam = 1;
											}
										}

										break;

									case 1:   //one value
										if(strcmp(varParam, "") != 0)
										{
											ivarParam = atof(varParam);
											iValue = atof(Value);
											PosLimit = iValue + (iValue*tolpercent); //Ex: 6:+3%
											NegLimit = iValue - (iValue*tolpercent); //Ex: 6:-3%
											
											if(NegLimit<0) NegLimit = 0;

											if(ivarParam >= NegLimit && ivarParam <= PosLimit)
											{
												validframes++;
												//printf("***8***\n");
											}
											else
											{
												notvalidframes++;
												fprintf(file_handle, "The expected value - %s : +/- %f - does not match to the received value - %s - in the line %d\n", Value, tolValue, varParam, i);    //range
												failedParam = 1;
											}
										}
										break;

									case 2:   //list of values
										if(strcmp(varParam, "") != 0)
										{
											ivarParam = atof(varParam);

											for(int m = 0; m<nb; m++)
											{
												dValMax[m] = dVal[m] + (dVal[m]*tolpercent);;
												dValMin[m] = dVal[m] - (dVal[m]*tolpercent);;
												//m++;
											}
											//compare values
											for(int l=0; l<nb; l++)
											{
												auxdValMax = dValMax[l];
												auxdValMin = dValMin[l];
												
												if(auxdValMin<0) auxdValMin = 0;

												if(ivarParam >= auxdValMin  && ivarParam <= auxdValMax)
												{
													//do nothing - limit OK
													valide++;
												}
												else notvalide++;
											}
											if(notvalide == nb)
											{
												notvalidframes++;
												fprintf(file_handle, "The expected value - %s : +/- %f - does not match to the received value - %s - in the line %d\n", Value, tolpercent, varParam, i);    //range
												failedParam = 1;
												notvalide=0;
											}
											else
											{
												validframes++;
												//printf("***9***\n");
												notvalide=0;
											}
										}
										break;
								}


							}
						}
					}
					if(strcmp(CKSGood, "0") == 0)
					{
						CKSGoodinvalid++ ;
						zerofound = 1;
						fprintf(file_handle, "CKSGood: 0 was found in line %d\n", i);
					}

					sprintf(rangeA, "A%d", i);
					ExcelRpt_GetCellValue (worksheetHandle9, rangeA, CAVT_CSTRING, _ATimeColumn);
					
				
						
					if(strcmp(label2, _ATimeColumn) == 0)   //stop condition check
					{
					//	printf("lab11 = %d and lab21 = %d\n",FirstLab,TwoLabs);
						if (noLab == 1 || FirstLab==1  || TwoLabs==1)
						{
						//printf("\n%s and %s\n",label2,_ATimeColumn);
						if(zerofound == 1)
						{
							float percentageCKSGood = ((double)CKSGoodinvalid/( (double)CKSGoodinvalid+ (double)CKSGoodvalid))*100.0;
							fprintf(file_handle, "\n%.2f%% of the CKSGood are equal to 0!\n\n",percentageCKSGood );
						}


						endCondition = 0; //stop analysis
						end = 0;
						if(failedParam == 0)
						{
							varCheckFieldValue = 0;// CheckFieldValue is passed
							fprintf(file_handle, "All values received correspond to expected values\n");
							//fprintf(file_handle, "CheckFieldValue ... passed!\n\n");
						}
						else
						{
							//fprintf(file_handle, "CheckFieldValue ... failed!\n\n");
							varCheckFieldValue = 1; // CheckFieldValue is failed
						}
						}
						noLab = 1;
					}
					
					}
				}
			}
		}
	
		end=1;

		//return varCheckFieldValue;  // 0 check OK
		// 1 check not OK
	}
	else
	{
		fprintf(file_handle, "The CheckFieldValue was not performed!\n\n");
		varCheckFieldValue =  -1;
	}

	printCheckResults (file_handle, "CheckFieldValue", varCheckFieldValue, validframes, notvalidframes, 0, 0, 0,0.0,0.0,0.0);
	return  varCheckFieldValue;
}



//***********************************************************************************************
// Modif - Carolina 05/2019
// iaverage, passTranslatedFrameNumber, passTranslatedAngleP, realFCParam, FunctionCodeValue, realParam, pathLogLabel, CurrentPath
//***********************************************************************************************
/*int CheckSTDEV(char * wuID, char *Value, double iaverage, char *sFrameNb, char *sAngleEmission, char *realFCParam, char *FunctionCodeValue, char *Parametertocheck)  //char *pathname2
{
	int boucle = 0, end = 1, endCondition = 1, zerofound = 0;
	char *CheckParam;
	char *CheckParamFC;
	char CKSGood[3] = "";
	char _ATimeColumn[255] = "";
	char _FunctionCode[50] = "";
	char range[30] = "";
	char rangeA[30] = "";
	char rangeFC[30] = "";
	char rangeFN[30] = "";
	char GoodSumRange[30] = "";
	char foundCellRange[30] = "";
	char FCRow[30] = "";
	char foundCellGoodSumRange[30] = "";
	char cellParamRange[30] = "";
	char cellFCRange[30] = "";
	char cellGoodSumRange[30] = "";
	char cellFrameNBRange[30]="";
	char cellAngle_position_Range[30] = "";
	char varParam[100]="";
	char Rowparam[255] = 		"/ROW/@";  //for parameter to check
	char Rowparam2[255] = 		"/ROW/@";  //for FC
	char CheckGoodsum[100] = 	"/ROW/@CKSGood";
	char Frame_number[100] = 	"/ROW/@Frame_number";
	char Angle_position[100] =  "/ROW/@Angle_position";
	char FrameNBRow[30] = "";
	char Angle_position_Row[30] = "";
	double iValue = 0.0;
	double sum = 0.0;
	int count = 0;
	double distance = 0.0;
	char iFrameNB[5] = "";
	char varFramNumber[5] = "";
	//char varAngle_position[30] = "";
	char auxAngleEmission[10] = "";
	int iAngleEmission = 0;
	double result = 0.0;
	int failResult = 0;
	int itest = 0;
	double dRange1 = 0.0, dRange2;

	int CKSGoodvalid = 0;
	int CKSGoodinvalid = 0;
	
	char CheckWUID[30] = 		"/ROW/@ID"; 
	char _WUIDColumn[255] = "";
	char WUIDRange[30] = ""; 
	char cellWUIDRange[30] = ""; 
	char WUID[30] = ""; 
	int  noLab=1; 


	//angle value 0 - 360
	RetrieveandSeparateRange(Value); // 0-4;
	dRange1 = atof(range1); //range1 and range2 are global variables
	dRange2 = atof(range2);

	//head of the check
	fprintf(file_handle, "\n****************CheckSTDEV****************\n");
	fprintf(file_handle, "                 Check_%d\n", NBTestCheck);
	fprintf(file_handle, "Standard deviation of the angle emission\n");

	CheckParamFC = strcat(Rowparam2, realFCParam); //EX: /ROW/@Function_code

	//check param
	CheckParam = strcat(Rowparam, Parametertocheck);  //Ex CheckParam = /ROW/@Channel
	//take excel file

	ExcelRpt_GetWorksheetFromIndex(workbookHandle9, 1, &worksheetHandle9);  //Worksheet Configuration

	// 1 - looking for "/ROW/@CKSGood"
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckGoodsum, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,foundCellGoodSumRange);
	char caracter1[] = "$";  			//foundCellRange =  $P$2
	char *token1 = strtok(foundCellGoodSumRange, caracter1);   //token1 = C
	if(token1 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckGoodsum);
	else
		strcpy(cellGoodSumRange, token1);

	//2 - looking for "/ROW/@Function_code" = $W$2
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckParamFC, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,FCRow);
	char caracter2[] = "$";
	char *token2 = strtok(FCRow, caracter2);
	if(token2 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckParamFC);
	else
		strcpy(cellFCRange, token2); 		// cellFCRange = W2

	//3 - looking for "/ROW/@Frame_number" = $U$2
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, Frame_number, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,FrameNBRow);
	char caracter3[] = "$";
	char *token3 = strtok(FrameNBRow, caracter3);
	if(token3 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", Frame_number);
	else
	{
		strcpy(cellFrameNBRange, token3); 	// cellFCRange = U2
		strcpy(iFrameNB, sFrameNb); 		//iFrameNB = "2"
	}

	//4 - looking for "/ROW/@Angle_position" = $F$2
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, Angle_position, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,Angle_position_Row);
	char caracter4[] = "$";
	char *token4 = strtok(Angle_position_Row, caracter4);
	if(token4 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", Angle_position);
	else
	{
		strcpy(cellAngle_position_Range, token4); 	// cellAngle_position_Range = F2
		strcpy(auxAngleEmission, sAngleEmission);
		iAngleEmission = atoi(auxAngleEmission);//iAngleEmission = "4" or 1 or 2
	}

	//5 - looking for "/ROW/@parameter"
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckParam, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,foundCellRange);
	char caracter5[] = "$";  //EX: foundCellRange =  $M$2
	char *token5 = strtok(foundCellRange, caracter5);
	if(token5 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckParam);
	else
		strcpy(cellParamRange, token5);   // cellParamRange = M2


	//6 - looking for "/ROW/@ID" = $W$2 // the WU ID
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckWUID, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,WUID);
	char caracterWUID[] = "$";
	char *tokenWUID = strtok(WUID, caracterWUID);
	if(tokenWUID == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckWUID);
	else
		strcpy(cellWUIDRange, tokenWUID);
	

	if(token1 != NULL && token2 != NULL && token3 != NULL && token4 != NULL && token5 != NULL && tokenWUID != NULL)
	{
		if (lab2 == 0)
		{
			if (lab1 == 0)
			{
			fprintf(file_handle, "Between %s (%d ms) and %s (%d ms)\n", 	label1, Time1, label2, Time2);
			}
			else
			{
			fprintf(file_handle, "Between the beginning and %s (%d ms)\n", label2, Time2);
			}
		}
		else
		{
		   	if (lab1 == 0)
			{
			fprintf(file_handle, "Between %s (%d ms) and the end of the script\n", 	label1, Time1);
			}
			else
			{
			fprintf(file_handle, "Between the beginning and the end of the script\n");
			}
		}
		fprintf(file_handle, "Function code: %s\n",FunctionCodeValue);
		fprintf(file_handle, "Frame number: %s\n", iFrameNB);
		fprintf(file_handle, "Angle emission: %d\n\n", iAngleEmission);
		fprintf(file_handle, "Range1 = %f and Range2 = %f\n",dRange1, dRange2 );  

		//vefirifier  entre labels
		//parcourir column to find time
		for(boucle=2; end==1; boucle++)
		{
			//until find label1
			sprintf(rangeA, "A%d", boucle+1);
			ExcelRpt_GetCellValue (worksheetHandle9, rangeA, CAVT_CSTRING, _ATimeColumn);

			if(strcmp(label1, _ATimeColumn) == 0 || lab1==1)
			{
				//analyse until find label 2
				for(int i = boucle+1 ; endCondition == 1; i++)
				{
					//1 - check Goodsum first
					sprintf(GoodSumRange,"%s%d",cellGoodSumRange, i); //P3
					ExcelRpt_GetCellValue (worksheetHandle9, GoodSumRange, CAVT_CSTRING, CKSGood);

					if(strcmp(CKSGood, "1") == 0)
					{
						
						// we look for the good WU ID
						sprintf(WUIDRange,"%s%d",cellWUIDRange, i);
						ExcelRpt_GetCellValue (worksheetHandle9, WUIDRange, CAVT_CSTRING, _WUIDColumn);

						if(strcmp(_WUIDColumn, wuID) == 0)
						{


							CKSGoodvalid++ ;
							//2 - check FC second
							sprintf(rangeFC,"%s%d",cellFCRange, i);
							ExcelRpt_GetCellValue (worksheetHandle9, rangeFC, CAVT_CSTRING, _FunctionCode);

							if(strcmp(_FunctionCode, FunctionCodeValue) == 0)
							{
								//3 - Check Frame Number
								sprintf(rangeFN,"%s%d",cellFrameNBRange, i);
								ExcelRpt_GetCellValue (worksheetHandle9, rangeFN, CAVT_CSTRING, varFramNumber);
								if(strcmp(varFramNumber, iFrameNB) == 0)
								{
									switch(iAngleEmission)
									{
										case 1:
											sprintf(range,"%s%d",cellParamRange, i);
											ExcelRpt_GetCellValue (worksheetHandle9, range, CAVT_CSTRING, varParam);
											//varParam is an angle
											//verifie quadrant
											iValue = atof(varParam);

											if(iValue >=0 && iValue <= 90)
											{
												iValue = iValue;
												distance = ((iValue - iaverage)*(iValue - iaverage));
											}
											if(iValue > 90 && iValue <= 180)
											{
												iValue = iValue - 90;
												distance = ((iValue - iaverage)*(iValue - iaverage));
											}
											if(iValue > 180 && iValue <= 270)
											{
												iValue = iValue - 180;
												distance = ((iValue - iaverage)*(iValue - iaverage));
											}
											if(iValue > 270 && iValue < 360)
											{
												iValue = iValue - 270;
												distance = ((iValue - iaverage)*(iValue - iaverage));
											}
											if(iValue >= 360)
											{
												iValue = iValue - 360;
												distance = ((iValue - iaverage)*(iValue - iaverage));
											}

											count++;
											sum = sum + distance;

											break;

										case 2:
											sprintf(range,"%s%d",cellParamRange, i);
											ExcelRpt_GetCellValue (worksheetHandle9, range, CAVT_CSTRING, varParam);
											//varParam is an angle
											//verifie quadrant
											iValue = atof(varParam);

											if(iValue >=0 && iValue <= 90)
											{
												iValue = iValue;
												distance = ((iValue - iaverage)*(iValue - iaverage));
											}
											if(iValue > 90 && iValue <= 180)
											{
												iValue = iValue - 90;
												distance = ((iValue - iaverage)*(iValue - iaverage));
											}
											if(iValue > 180 && iValue <= 270)
											{
												iValue = iValue - 180;
												distance = ((iValue - iaverage)*(iValue - iaverage));
											}
											if(iValue > 270 && iValue < 360)
											{
												iValue = iValue - 270;
												distance = ((iValue - iaverage)*(iValue - iaverage));
											}
											if(iValue >= 360)
											{
												iValue = iValue - 360;
												distance = ((iValue - iaverage)*(iValue - iaverage));
											}

											count++;
											sum = sum + distance;

											break;

										case 4:
											//check param and take its value
											sprintf(range,"%s%d",cellParamRange, i);
											ExcelRpt_GetCellValue (worksheetHandle9, range, CAVT_CSTRING, varParam);
											//varParam is an angle
											//verifie quadrant
											iValue = atof(varParam);

											if(iValue >=0 && iValue <= 90)
											{
												iValue = iValue;
												distance = ((iValue - iaverage)*(iValue - iaverage));
												//fprintf(file_handle, "value is: %f\n", distance);
											}
											if(iValue > 90 && iValue <= 180)
											{
												iValue = iValue - 90;
												distance = ((iValue - iaverage)*(iValue - iaverage));
												//fprintf(file_handle, "value is: %f\n", distance);
											}
											if(iValue > 180 && iValue <= 270)
											{
												iValue = iValue - 180;
												distance = ((iValue - iaverage)*(iValue - iaverage));
												//	fprintf(file_handle, "value is: %f\n", distance);
											}
											if(iValue > 270 && iValue < 360)
											{
												iValue = iValue - 270;
												distance = ((iValue - iaverage)*(iValue - iaverage));
												//	fprintf(file_handle, "value is: %f\n", distance);
											}
											if(iValue >= 360)
											{
												//iValue = iValue - 360;
												itest = (iValue/360); //nb of turns
												iValue = iValue - itest*360;
												distance = ((iValue - iaverage)*(iValue - iaverage));
												//	fprintf(file_handle, "value is: %f\n", distance);
											}

											count++;
											sum = sum + distance;

											break;
									}  //switch
								}  // Frame Number
							}  //check FC second

						}

					} //Goodsum
					if(strcmp(CKSGood, "0") == 0)
					{
						CKSGoodinvalid++ ;
						zerofound = 1;
						fprintf(file_handle, "CKSGood: 0 was found in line %d\n", i);
					}


					sprintf(rangeA, "A%d", i);
					ExcelRpt_GetCellValue (worksheetHandle9, rangeA, CAVT_CSTRING, _ATimeColumn);
					if(strcmp(label2, _ATimeColumn) == 0)   //stop condition check
					{
						if (noLab == 1 || FirstLab==1  || TwoLabs==1)
						{
						if(zerofound == 1)
						{
							float percentageCKSGood = ((double)CKSGoodinvalid/( (double)CKSGoodinvalid+ (double)CKSGoodvalid))*100.0;
							fprintf(file_handle, "\n%.2f%% of the CKSGood are equal to 0!\n\n",percentageCKSGood );
						}

						endCondition = 0; //stop analysis
						end = 0;
						//Average = sum/count;
						if(count > 0)
						{
							result = 3*sqrt(sum/count);

							if(result < dRange1 || result > dRange2)  //range
							{
								//faild
								failResult = 1;
								fprintf(file_handle, "Range1 = %f and Range2 = %f\n",dRange1, dRange2 );
								fprintf(file_handle, "The Standard deviation is: %f\n", result);
								fprintf(file_handle, "CheckSTDEV_%d ... failed!\n\n", NBTestCheck);
							}
							else
							{
								//succes 3*sigma between the ranges
								failResult = 0;
								fprintf(file_handle, "The stardard deviation is: %f\n", result);
								fprintf(file_handle, "CheckSTDEV_%d ... passed!\n\n", NBTestCheck);

							}

						}
						else
						{
							failResult = 1;
							fprintf(file_handle, "There are no emissons between the labels");
						}
						}
						noLab=1;
					} //if label2

				}  //2nd boucle for
			} //1st label1
		}
		end=1;

		//restart time1 and time2
		
	}
	else
	{
		fprintf(file_handle, "The CheckSTDEV was not performed!\n\n");
		failResult =  -1;
	}

	//printCheckResults(file_handle, "CheckSTDEV" , failResult, result, 0, 0, 0, 0);
	printCheckResults(file_handle, "CheckSTDEV" , 0, 0, 0, 0, 0, 0,result,dRange2,dRange1);  
	return failResult;

}*/

//***********************************************************************************************
// Modif - Carolina 05/2019
// Fist filter: takes parameter value of the function code determined between labels
// second: calculate average of these values
// return a doc
//***********************************************************************************************
/*double CheckAverage(char * wuID, char *sFrameNb, char *sAngleEmission, char *realFCParam, char *FunctionCodeValue, char *Parametertocheck)
{
	int boucle = 0, end = 1, endCondition = 1, zerofound = 0;		//char *fichierLogxlsx2,    char *pathname2
	char *CheckParam;
	char *CheckParamFC;
	char CKSGood[3] = "";
	char _ATimeColumn[255] = "";
	char _FunctionCode[50] = "";
	char range[30] = "";
	char rangeA[30] = "";
	char rangeFC[30] = "";
	char rangeFN[30] = "";
	//char rangeAP[30] = "";
	char GoodSumRange[30] = "";
	char foundCellRange[30] = "";
	char FCRow[30] = "";
	char foundCellGoodSumRange[30] = "";
	char cellParamRange[30] = "";
	char cellFCRange[30] = "";
	char cellGoodSumRange[30] = "";
	//static FILE *file_handle;
	char varParam[100]="";
	char Rowparam[255] = 		"/ROW/@";  //for parameter to check
	char Rowparam2[255] = 		"/ROW/@";  //for FC
	char CheckGoodsum[100] = 	"/ROW/@CKSGood";
	char Frame_number[100] = 	"/ROW/@Frame_number";
	char FrameNBRow[30] = "";
	//char *auxFrameNB;
	char cellFrameNBRange[30]="";
	double iValue = 0.0;
	double sum = 0.0;
	int count = 0;
	double Average = 0.0;
	double findmin = 5000.0;
	double findmax = 0.0;
	char iFrameNB[5] = "";
	char varFramNumber[5] = "";
	char Angle_position[100] =  "/ROW/@Angle_position";
	char Angle_position_Row[30] = "";
	char cellAngle_position_Range[30] = "";
	//char varAngle_position[30] = "";
	char auxAngleEmission[10] = "";
	int iAngleEmission = 0;
	int itest = 0;

	int CKSGoodvalid = 0;
	int CKSGoodinvalid = 0;

	char CheckWUID[30] = 		"/ROW/@ID"; 
	char _WUIDColumn[255] = "";
	char WUIDRange[30] = ""; 
	char cellWUIDRange[30] = ""; 
	char WUID[30] = ""; 


	//head of the file check
	fprintf(file_handle, "\n***************CheckAverage***************\n");
	fprintf(file_handle, "                 Check_%d\n", NBTestCheck);

	CheckParamFC = strcat(Rowparam2, realFCParam); //EX: /ROW/@Function_code

	//check param
	CheckParam = strcat(Rowparam, Parametertocheck);  //Ex CheckParam = /ROW/@Channel
	//take excel file
	ExcelRpt_GetWorksheetFromIndex(workbookHandle9, 1, &worksheetHandle9);  //Worksheet Configuration

	// 1 - looking for "/ROW/@CKSGood"
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckGoodsum, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,foundCellGoodSumRange);
	char caracter1[] = "$";  //foundCellRange =  $P$2
	char *token1 = strtok(foundCellGoodSumRange, caracter1);   //token1 = C
	if(token1 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckGoodsum);
	else
		strcpy(cellGoodSumRange, token1);

	//2 - looking for "/ROW/@Function_code" = $W$2
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckParamFC, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,FCRow);
	char caracter2[] = "$";
	char *token2 = strtok(FCRow, caracter2);
	if(token2 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckParamFC);
	else
		strcpy(cellFCRange, token2); // cellFCRange = W2

	//3 - looking for "/ROW/@Frame_number" = $U$2
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, Frame_number, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,FrameNBRow);
	char caracter3[] = "$";
	char *token3 = strtok(FrameNBRow, caracter3);
	if(token3 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", Frame_number);
	else
	{
		strcpy(cellFrameNBRange, token3); 	// cellFCRange = U2
		strcpy(iFrameNB, sFrameNb); 		//iFrameNB = "2"
	}

	//4 - looking for "/ROW/@Angle_position" = $F$2
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, Angle_position, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,Angle_position_Row);
	char caracter4[] = "$";
	char *token4 = strtok(Angle_position_Row, caracter4);
	if(token4 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", Angle_position);
	else
	{
		strcpy(cellAngle_position_Range, token4); 	// cellAngle_position_Range = F2
		strcpy(auxAngleEmission, sAngleEmission);
		iAngleEmission = atoi(auxAngleEmission);//iAngleEmission = "4" or 1 or 2
	}

	//5 - looking for "/ROW/@parameter"
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckParam, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,foundCellRange);
	char caracter5[] = "$";  //EX: foundCellRange =  $M$2
	char *token5 = strtok(foundCellRange, caracter5);
	if(token5 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckParam);
	else
		strcpy(cellParamRange, token5);   // cellParamRange = M2
	
	//6 - looking for "/ROW/@ID" = $W$2 // the WU ID
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckWUID, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,WUID);
	char caracterWUID[] = "$";
	char *tokenWUID = strtok(WUID, caracterWUID);
	if(tokenWUID == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckWUID);
	else
		strcpy(cellWUIDRange, tokenWUID);	
	


	if(token1 != NULL && token2 != NULL && token3 != NULL && token4 != NULL && token5 != NULL && tokenWUID != NULL)
	{
		fprintf(file_handle, "\nAverage of the parameter: Angle emission\nBetween %s (%d ms) and %s (%d ms)\nFunction Code value: %s\n\n", label1, Time1, label2, Time2, FunctionCodeValue);

		
		
		
		//vefirifier  entre labels
		//parcourir column to find time
		for(boucle=2; end==1; boucle++)
		{
			//until find label1
			sprintf(rangeA, "A%d", boucle+1);
			ExcelRpt_GetCellValue (worksheetHandle9, rangeA, CAVT_CSTRING, _ATimeColumn);

			if(strcmp(label1, _ATimeColumn) == 0)
			{
				//analyse until find label 2
				for(int i = boucle+1 ; endCondition == 1; i++)
				{
					//1 - check Goodsm first
					sprintf(GoodSumRange,"%s%d",cellGoodSumRange, i); //P3
					ExcelRpt_GetCellValue (worksheetHandle9, GoodSumRange, CAVT_CSTRING, CKSGood);

					if(strcmp(CKSGood, "1") == 0)
					{
						
						// we look for the good WU ID
						sprintf(WUIDRange,"%s%d",cellWUIDRange, i);
						ExcelRpt_GetCellValue (worksheetHandle9, WUIDRange, CAVT_CSTRING, _WUIDColumn);

						if(strcmp(_WUIDColumn, wuID) == 0)
						{
						
							CKSGoodvalid++ ;
							//2 - check FC second
							sprintf(rangeFC,"%s%d",cellFCRange, i);
							ExcelRpt_GetCellValue (worksheetHandle9, rangeFC, CAVT_CSTRING, _FunctionCode);

							if(strcmp(_FunctionCode, FunctionCodeValue) == 0)
							{
								//3 - Check Frame Number
								sprintf(rangeFN,"%s%d",cellFrameNBRange, i);
								ExcelRpt_GetCellValue (worksheetHandle9, rangeFN, CAVT_CSTRING, varFramNumber);
								if(strcmp(varFramNumber, iFrameNB) == 0)
								{
									//angle emission 1,2,3,4
									switch(iAngleEmission)
									{
										case 1:
											sprintf(range,"%s%d",cellParamRange, i);
											ExcelRpt_GetCellValue (worksheetHandle9, range, CAVT_CSTRING, varParam);
											//varParam is an angle
											//verifie quadrant
											iValue = atof(varParam);
											if(findmin >= iValue )
											{
												findmin = iValue;   //find min value between labels
											}
											if(findmax <= iValue )
											{
												findmax = iValue;   //find max value between labels
											}

											if(iValue >=0 && iValue <= 90)
											{
												iValue = iValue;
											}
											if(iValue > 90 && iValue <= 180)
											{
												iValue = iValue - 90;
											}
											if(iValue > 180 && iValue <= 270)
											{
												iValue = iValue - 180;
											}
											if(iValue > 270 && iValue < 360)
											{
												iValue = iValue - 270;
											}
											if(iValue >= 360)
											{
												itest = (iValue/360); //nb of turns
												iValue = iValue - itest*360;
											}

											count++;
											sum = sum + iValue;

											break;

										case 2:
											sprintf(range,"%s%d",cellParamRange, i);
											ExcelRpt_GetCellValue (worksheetHandle9, range, CAVT_CSTRING, varParam);
											//varParam is an angle
											//verifie quadrant
											iValue = atof(varParam);
											if(findmin >= iValue )
											{
												findmin = iValue;   //find min value between labels
											}
											if(findmax <= iValue )
											{
												findmax = iValue;   //find max value between labels
											}

											if(iValue >=0 && iValue <= 90)
											{
												iValue = iValue;
											}
											if(iValue > 90 && iValue <= 180)
											{
												iValue = iValue - 90;
											}
											if(iValue > 180 && iValue <= 270)
											{
												iValue = iValue - 180;
											}
											if(iValue > 270 && iValue < 360)
											{
												iValue = iValue - 270;
											}
											if(iValue >= 360)
											{
												itest = (iValue/360); //nb of turns
												iValue = iValue - itest*360;
											}

											count++;
											sum = sum + iValue;

											break;

										case 4:
											//check param and take its value
											sprintf(range,"%s%d",cellParamRange, i);
											ExcelRpt_GetCellValue (worksheetHandle9, range, CAVT_CSTRING, varParam);
											//varParam is an angle
											//verifie quadrant
											fprintf(file_handle, "%s\n",varParam);
											iValue = atof(varParam);
											if(findmin >= iValue )
											{
												findmin = iValue;   //find min value between labels
											}
											if(findmax <= iValue )
											{
												findmax = iValue;   //find max value between labels
											}

											if(iValue >=0 && iValue <= 90)
											{
												iValue = iValue;
												//	fprintf(file_handle, "value is: %f\n", iValue);
											}
											if(iValue > 90 && iValue <= 180)
											{
												iValue = iValue - 90;
												//	fprintf(file_handle, "value is: %f\n", iValue);
											}
											if(iValue > 180 && iValue <= 270)
											{
												iValue = iValue - 180;
												//	fprintf(file_handle, "value is: %f\n", iValue);
											}
											if(iValue > 270 && iValue < 360)
											{
												iValue = iValue - 270;
												//	fprintf(file_handle, "value is: %f\n", iValue);
											}
											if(iValue >= 360)
											{
												itest = (iValue/360); //nb of turns
												iValue = iValue - itest*360;
												//	fprintf(file_handle, "value is: %f\n", iValue);
											}

											count++;
											sum = sum + iValue;

											break;
									}
								}
							}
							
						}
					}
					if(strcmp(CKSGood, "0") == 0)
					{
						CKSGoodinvalid++ ;
						zerofound = 1;
						fprintf(file_handle, "CKSGood: 0 was found in line %d\n", i);
					}


					sprintf(rangeA, "A%d", i);
					ExcelRpt_GetCellValue (worksheetHandle9, rangeA, CAVT_CSTRING, _ATimeColumn);

					if(strcmp(label2, _ATimeColumn) == 0)   //stop condition check
					{
						if(zerofound == 1)
						{
							float percentageCKSGood = ((double)CKSGoodinvalid/( (double)CKSGoodinvalid+ (double)CKSGoodvalid))*100.0;
							fprintf(file_handle, "\n%.2f%% of the CKSGood are equal to 0!\n\n",percentageCKSGood );
						}


						endCondition = 0; //stop analysis
						end = 0;
						if(count > 0)
						{
							Average = sum/count;

							fprintf(file_handle, "The Avarage of the values is: %f\n", Average);
							fprintf(file_handle, "The max value (absolute) between labels is: %f\n", findmax);
							fprintf(file_handle, "The min value (absolute) between labels is: %f\n\n", findmin);
						}
						else
						{
							Average = 0.0;
							findmax = 0.0;
							findmin = 0.0;
							fprintf(file_handle, "The Avarage of the values is: %f\n", Average);
							fprintf(file_handle, "The max value (absolute) between labels is: %f\n", findmax);
							fprintf(file_handle, "The min value (absolute) between labels is: %f\n\n", findmin);
						}
					}
				}
			}
		}

		end=1;
		//fclose (file_handle);
		//Time1=0;
		//Time2=0;
		
	}
	else
	{
		//Time1=0;
		//Time2=0;
		fprintf(file_handle, "The CheckAverage was not performed!\n\n");
		Average = -1;
	}
	
	//printCheckResults(file_handle, "CheckAverage" , Average, findmax, findmin, 0, 0, 0);
	printCheckResults(file_handle, "CheckAverage" , 0, 0, 0, 0, 0, 0,Average, findmax, findmin);
	return Average;
}
*/
//***********************************************************************************************
// Modif - Carolina 06/2019
// Finding RF signal
//***********************************************************************************************
int findRFsignal(char *channel)		 //  findValidRFsignal
{
	int RFfound = 0;
	char *  sRFfound;

	//printf("channel = %s\n",channel);
	sRFfound = strstr(channel,"RF");

	if (sRFfound != NULL)
	{
		RFfound = 1;
	}

	return RFfound;


}

//***********************************************************************************************
// Modif - Carolina 06/2019
// Check if there is no RF response
//
//***********************************************************************************************
int CheckNoRF(char * wuID, char* InterfacetoCheck)
{
	int boucle = 0, end = 1, endCondition = 1, zerofound = 0, RFfound=0, tokennull = 0, testFailed = 0;
	char CheckChannel[15] = 	"/ROW/@Channel";
	char CheckGoodsum[15] = 	"/ROW/@";
	char CKSGood[3] = "";
	char Channel[255] = "";

	char Channelaux[255] = "";
	char _ATimeColumn[255] = "";
	//char range[30] = "";
	char rangeA[30] = "";
	
	char GoodSumRange[30] = "";
	char ChannelRange[30] = "";
	char foundCellGoodSumRange[30] = "";
	char foundCellChannelRange[30] = "";
	char cellGoodSumRange[30] = "";
	char cellChannelRange[30] = "";
	int CheckNORF = 0;
	int counNbofFrames = 0;

	int CKSGoodvalid = 0;
	int CKSGoodinvalid = 0;
	int noLab=0;
	
	char CheckWUID[30] = 		"/ROW/@"; 
	char _WUIDColumn[255] = "";
	char WUIDRange[30] = ""; 
	char cellWUIDRange[30] = ""; 
	char WUID[30] = ""; 
	
	
	char CheckDesc[15] = 	"/ROW/@Desc";
	char RFProt[30]="";
	char cellRFProtocol[30]="";
	char RFprotocol[255]="";
	char RFrange[30]="";
	//strcat(RFprotocol,translateData(InterfacetoCheck, "Desc"));
	// enlever le Desc=" et le " de la fin !!!
	strcat(CheckGoodsum,translateData(InterfacetoCheck, "CKS_Validity"));
	strcat(CheckWUID,translateData(InterfacetoCheck,"ID"));
	/*strcat(CheckGoodsum,StandardToAttribute("CKS_Validity"));
	strcat(CheckWUID,StandardToAttribute("ID"));*/
	//printf("CheckWUID = %s\n",CheckWUID);
	//printf("CheckGoodsum = %s\n",CheckGoodsum); 

	

	//Worksheet Logs
	ExcelRpt_GetWorksheetFromIndex(workbookHandle9, 1, &worksheetHandle9);

	fprintf(file_handle, "\n*****************CheckNoRF*****************\n");
	fprintf(file_handle, "                 Check_%d\n\n", NBTestCheck);

	//3 - looking for "/ROW/@Desc" 
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckDesc, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,RFProt);
	char caracterDesc[] = "$";
	char *tokenDesc = strtok(RFProt, caracterDesc);
	if(tokenDesc == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckDesc);
	else
		strcpy(cellRFProtocol, tokenDesc);
	
	// 1 - looking for "/ROW/@CKSGood"
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckGoodsum, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,foundCellGoodSumRange);
	if(strcmp(foundCellGoodSumRange , "") == 0)
	{
		tokennull = 0;
	}
	else
	{
		char caracter1[] = "$";  //foundCellRange =  $O$2
		char *token1 = strtok(foundCellGoodSumRange, caracter1);   //token1 = O
		if(token1 == NULL)
		{
			fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckGoodsum);
			tokennull = 0;
		}
		else
		{
			strcpy(cellGoodSumRange, token1);
			tokennull = 1;
		}
	}

	// 2 - looking for "/ROW/@Channel"
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckChannel, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,foundCellChannelRange);
	char caracter2[] = "$";  //foundCellRange =  $L$2
	char *token2 = strtok(foundCellChannelRange, caracter2);   //token1 = L
	if(token2 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckChannel);
	else
		strcpy(cellChannelRange, token2);

	//3 - looking for "/ROW/@ID" = $W$2 // the WU ID
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckWUID, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,WUID);
	char caracterWUID[] = "$";
	char *tokenWUID = strtok(WUID, caracterWUID);
	if(tokenWUID == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckWUID);
	else
		strcpy(cellWUIDRange, tokenWUID);
	
	

	if(token2 != NULL && tokenWUID != NULL)
	{
			if (lab2 == 0)
		{
			if (lab1 == 0)
			{
			fprintf(file_handle, "Between %s (%d ms) and %s (%d ms)\n", 	label1, Time1, label2, Time2);
			}
			else
			{
			fprintf(file_handle, "Between the beginning and %s (%d ms)\n", label2, Time2);
			}
		}
		else
		{
		   	if (lab1 == 0)
			{
			fprintf(file_handle, "Between %s (%d ms) and the end of the script\n", 	label1, Time1);
			}
			else
			{
			fprintf(file_handle, "Between the beginning and the end of the script\n");
			}
		}
		

		for(boucle=2; end==1; boucle++)
		{
			//until find label1
			sprintf(rangeA, "A%d", boucle+1);
			ExcelRpt_GetCellValue (worksheetHandle9, rangeA, CAVT_CSTRING, _ATimeColumn);

			if(strcmp(label1, _ATimeColumn) == 0 || lab1==1)
			{
				//analyse until find label 2
				for(int i = boucle+1 ; endCondition == 1; i++)
				{
				
				sprintf(RFrange, "%s%d", cellRFProtocol, i);
				ExcelRpt_GetCellValue (worksheetHandle9, RFrange, CAVT_CSTRING,RFprotocol);
				    
				if (strcmp(InterfacetoCheck,"") == 0 || CheckRFProtocol(InterfacetoCheck,RFprotocol) == 1 )
				{
					//1 - check Channel column
					sprintf(ChannelRange,"%s%d",cellChannelRange, i); //P3
					ExcelRpt_GetCellValue (worksheetHandle9, ChannelRange, CAVT_CSTRING, Channel);
					strcpy(Channelaux, Channel);

					if(strcmp(Channel, "") != 0) //cell null
					{
						RFfound = findRFsignal(Channel);
						if(RFfound == 1)   //if rf found -> check CheckSum to confirm
						{

							// we look for the good WU ID
							sprintf(WUIDRange,"%s%d",cellWUIDRange, i);
							ExcelRpt_GetCellValue (worksheetHandle9, WUIDRange, CAVT_CSTRING, _WUIDColumn);

							if(strcmp(_WUIDColumn, wuID) == 0)
							{

								// 2 - looking for "/ROW/@CKSGood" column

								counNbofFrames++;
								if(tokennull == 0)  //CheckGoodsum not found -> CheckNoRF ok
								{
									//fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckGoodsum);
									RFfound = 0;
									zerofound = 0;
								}
								if(tokennull == 1)
								{

									sprintf(GoodSumRange,"%s%d",cellGoodSumRange, i); //P3
									ExcelRpt_GetCellValue (worksheetHandle9, GoodSumRange, CAVT_CSTRING, CKSGood);

									if(strcmp(CKSGood, "1") == 0) //frame is good
									{
										testFailed = 1;
										CKSGoodvalid++ ;
										fprintf(file_handle, "RF %s found in line %d\n", Channelaux, i); //RF 2- found in line 19
										zerofound = 0;
									}
									if(strcmp(CKSGood, "0") == 0)  //frame is not good
									{
										CKSGoodinvalid++ ;
										zerofound = 1;
										//RFfound = 0;
										fprintf(file_handle, "\nCKSGood: 0 was found in line %d\n", i);
									}

								} //if token null

							}
						} //rf found
					} //if cmp channel

					sprintf(rangeA, "A%d", i);
					ExcelRpt_GetCellValue (worksheetHandle9, rangeA, CAVT_CSTRING, _ATimeColumn);

					if(strcmp(label2, _ATimeColumn) == 0)   //stop condition check
					{
						if (noLab == 1 || FirstLab==1  || TwoLabs==1)
						{
						/*if(zerofound == 0)
						{
							fprintf(file_handle, "\nCKSGood ... OK!\n");
						} */

						if(testFailed == 1 && zerofound == 0)
						{
							CheckNORF = 1;
							fprintf(file_handle, "\nRF frames received!\n");
						}
						if(testFailed == 0 && zerofound == 0)
						{
							CheckNORF = 0;
							fprintf(file_handle, "\nNo RF frames received!\n");
						}

						if(zerofound == 1)
						{
							float percentageCKSGood = ((double)CKSGoodinvalid/( (double)CKSGoodinvalid+ (double)CKSGoodvalid))*100.0;
							fprintf(file_handle, "\n%.2f%% of the CKSGood are equal to 0!\n\n",percentageCKSGood );
						}

						/*	else
							{
								fprintf(file_handle, "\nCheckNoRF ... failed!\n\n");

							}  */

						endCondition = 0; //stop analysis
						end = 0;
						}
						noLab=1;
					} //if label 2
				}
				} //for i
			} //if label 1
		}  //boucle
		end = 1;

		if (counNbofFrames > 0)
		{
			CheckNORF = 1;
		}



		//return RFfound;
	}  //if token2 null
	else
	{
		fprintf(file_handle, "The CheckNoRF was not performed!\n\n");
		CheckNORF = -1;
	}

	printCheckResults (file_handle, "CheckNoRF", CheckNORF, 0, 0, counNbofFrames, 0, 0,0.0,0.0,0.0);
	return CheckNORF;
}

//***********************************************************************************************
// Modif - Carolina 06/2019
// Check time response to LF
// return 1 if Timing > 4s
//***********************************************************************************************
int CheckTimingFirstRF(char * wuID, char *Value, char *FCParam, char *FunctionCode, double tolValue, double tolpercent, char* InterfacetoCheck)
{
	int boucle = 0, end = 1, endCondition = 1, zerofound = 0, resultFirstRF = 0, RFfound = 0;
	char CheckGoodsum[15] = 	"/ROW/@";
	char CheckChannel[15] = 	"/ROW/@Channel";
	char CheckRTime[15] = 		"/ROW/@";
	char CKSGood[3] = "";
	char _ATimeColumn[255] = "";
	char rangeA[30] = "";
	char GoodSumRange[30] = "";
	char foundCellGoodSumRange[30] = "";
	char cellGoodSumRange[30] = "";
	char CheckParamFC[255] = 		"/ROW/@";  //for FC
	char FCRow[30] = "";
	char cellFCRange[30] = "";
	//char *CheckParamFC;
	char rangeFC[30] = "";
	char _FunctionCode[50] = "";
	char cellChannelRange[30] = "";
	char foundCellChannelRange[30] = "";
	char Channel[255] = "";
	char ChannelRange[30] = "";
	char _RTimeColumn[255] = "";
	char cellTimeRange[30]="";
	char RTime[30] = "";
	char rangeTime[30]="";
	double dtime=0.0, TimingofFirstRF=0.0, dRange1=0.0, dRange2=0.0,
		   TimingofFirstRFMin = 0.0, TimingofFirstRFMax = 0.0, TimingofFirstRFMinpercent = 0.0, TimingofFirstRFMaxpercent = 0.0;
	char percent[4] = "[%]";

	int CheckTFRF =0;
	int CKSGoodvalid = 0;
	int CKSGoodinvalid = 0;
	int noTime = 0;
	char CheckWUID[30] = 		"/ROW/@"; 
	char _WUIDColumn[255] = "";
	char WUIDRange[30] = ""; 
	char cellWUIDRange[30] = ""; 
	char WUID[30] = "";
//	int found = 0;

	char CheckDesc[15] = 	"/ROW/@Desc";
	char RFProt[30]="";
	char cellRFProtocol[30]="";
	char RFprotocol[255]="";
	char RFrange[30]="";
	
	
	strcat(CheckGoodsum,translateData(InterfacetoCheck, "CKS_Validity"));
	strcat(CheckWUID,translateData(InterfacetoCheck,"ID"));
	strcat(CheckRTime,translateData(InterfacetoCheck, "Time"));
	//printf("CheckWUID = %s\n",CheckWUID);
	//printf("CheckRTime = %s\n",CheckRTime);
	//printf("CheckGoodsum = %s\n",CheckGoodsum); 

	RetrieveandSeparateRange(Value); 				// Ex 0-DPS_Interburst; or 0-16;
	dRange1 = atof(range1); 		//0
	dRange2 = atof(range2);		//16
	//time range then *1000
	dRange1 = dRange1 * 1000;						//0.0000
	dRange2 = dRange2 * 1000;						//16000.0000
	tolValue = tolValue * 1000;

	fprintf(file_handle, "\n***************CheckTimingFirstRF***************\n");
	fprintf(file_handle, "                 Check_%d\n\n", NBTestCheck);

	 strcat(CheckParamFC, FCParam); //EX: /ROW/@Function_code
	//Worksheet Logs
	ExcelRpt_GetWorksheetFromIndex(workbookHandle9, 1, &worksheetHandle9);

	// 1 - looking for "/ROW/@CKSGood"
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckGoodsum, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,foundCellGoodSumRange);
	char caracter1[] = "$";  //foundCellRange =  $O$2
	char *token1 = strtok(foundCellGoodSumRange, caracter1);   //token1 = O
	if(token1 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckGoodsum);
	else
		strcpy(cellGoodSumRange, token1);

	//2 - looking for "/ROW/@Function_code" = $W$2
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckParamFC, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,FCRow);
	char caracter2[] = "$";
	char *token2 = strtok(FCRow, caracter2);
	if(token2 == NULL)
		fprintf(file_handle, "The column %s was not found, RFprotocole \"%s\" not received!\n\n", CheckParamFC,InterfacetoCheck);
	else
		strcpy(cellFCRange, token2); // cellFCRange = W2

	// 3 - looking for "/ROW/@Channel"
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckChannel, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,foundCellChannelRange);
	char caracter3[] = "$";  //foundCellRange =  $L$2
	char *token3 = strtok(foundCellChannelRange, caracter3);   //token1 = L
	if(token3 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckChannel);
	else
		//printf("token3 = %s\n",token3);
		strcpy(cellChannelRange, token3);

	//4 - looking for "/ROW/@_RTime" = $B$2
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckRTime, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,RTime);
	char caracter4[] = "$";
	char *token4 = strtok(RTime, caracter4);
	if(token4 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckRTime);
	else
		strcpy(cellTimeRange, token4); // cellFCRange = B2
	
	//5 - looking for "/ROW/@ID" = $W$2 // the WU ID
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckWUID, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,WUID);
	char caracterWUID[] = "$";
	char *tokenWUID = strtok(WUID, caracterWUID);
	if(tokenWUID == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckWUID);
	else
		strcpy(cellWUIDRange, tokenWUID);

	//3 - looking for "/ROW/@Desc" 
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckDesc, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,RFProt);
	char caracterDesc[] = "$";
	char *tokenDesc = strtok(RFProt, caracterDesc);
	if(tokenDesc == NULL)
	{
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckDesc);
	}
	else
	{
	   strcpy(cellRFProtocol, tokenDesc); 
	}
	
	if(token1 != NULL && token2 != NULL && token3 != NULL && token4 != NULL && tokenWUID != NULL)
	{
		
		fprintf(file_handle, "Between %s and first RF\n", 		label1);
		fprintf(file_handle, "Function code: %s\n",				FunctionCode);
		fprintf(file_handle, "Expected range: %f:%f\n", 			dRange1,dRange2);
		fprintf(file_handle, "Tolerance value: %f\n", 			tolValue);
		fprintf(file_handle, "Tolerance %s: %f\n\n",			percent, tolpercent);


		for(boucle=2; end==1; boucle++)
		{
			//until find label1
			sprintf(rangeA, "A%d", boucle+1);
			ExcelRpt_GetCellValue (worksheetHandle9, rangeA, CAVT_CSTRING, _ATimeColumn);

			//starting check when label1 was found
			//printf("label1 = %s and _ATimeColumn = %s\n",label1,_ATimeColumn);  
		if(strcmp(label1, _ATimeColumn) == 0 || strcmp(label1,";") == 0)
		{  
				//analyse until find label 2
				for(int i = boucle+1 ; endCondition == 1; i++)
				{
					
					sprintf(RFrange, "%s%d", cellRFProtocol, i);
					ExcelRpt_GetCellValue (worksheetHandle9, RFrange, CAVT_CSTRING,RFprotocol);
				    
					if (strcmp(InterfacetoCheck,"") == 0 || CheckRFProtocol(InterfacetoCheck,RFprotocol) == 1 )
					{
					//1 - check Goodsm first
					sprintf(GoodSumRange,"%s%d",cellGoodSumRange, i); //P3
					ExcelRpt_GetCellValue (worksheetHandle9, GoodSumRange, CAVT_CSTRING, CKSGood);

					if(strcmp(CKSGood, "1") == 0)
					{

						// we look for the good WU ID
						sprintf(WUIDRange,"%s%d",cellWUIDRange, i);
						ExcelRpt_GetCellValue (worksheetHandle9, WUIDRange, CAVT_CSTRING, _WUIDColumn);

						if(strcmp(_WUIDColumn, wuID) == 0)
						{

							CKSGoodvalid++ ;
							//2 - check FC second
							sprintf(rangeFC,"%s%d",cellFCRange, i);
							ExcelRpt_GetCellValue (worksheetHandle9, rangeFC, CAVT_CSTRING, _FunctionCode);

							if(strcmp(_FunctionCode, FunctionCode) == 0)
							{
								//2 - check FirstRF
								sprintf(ChannelRange,"%s%d",cellChannelRange, i); //P3
								ExcelRpt_GetCellValue (worksheetHandle9, ChannelRange, CAVT_CSTRING, Channel);

								RFfound = findRFsignal(Channel);
								if(RFfound == 1)
								{
									//found = 1;
									//calculating time
									sprintf(rangeTime,"%s%d",cellTimeRange, i);
									ExcelRpt_GetCellValue (worksheetHandle9, rangeTime, CAVT_CSTRING, _RTimeColumn);
									dtime = atof(_RTimeColumn);
									TimingofFirstRF = dtime - Time1;
									//printf("dtime = _%f_ and Time1= _%d_ and TimingofFirstRF = _%f_\n",dtime,Time1,TimingofFirstRF);

									if(tolValue != 0 && tolpercent == 0)
									{
										TimingofFirstRFMin = TimingofFirstRF - tolValue;
										if(TimingofFirstRFMin<0)
											TimingofFirstRFMin=0;
										TimingofFirstRFMax = TimingofFirstRF + tolValue;

										if((TimingofFirstRFMin >= dRange1) && (TimingofFirstRFMax <= dRange2) )
										{
											resultFirstRF = 0;
											CheckTFRF = 0;
										}
										else
										{
											resultFirstRF = 1;
											CheckTFRF = 1;
										}
										endCondition = 0; //stop analysis
										end = 0;

									}
									else if(tolValue == 0 && tolpercent != 0)
									{
										TimingofFirstRFMinpercent = TimingofFirstRF - tolpercent;
										if(TimingofFirstRFMinpercent<0)
											TimingofFirstRFMinpercent=0;
										TimingofFirstRFMaxpercent = TimingofFirstRF + tolpercent;

										if((TimingofFirstRFMinpercent >= dRange1) && (TimingofFirstRFMaxpercent <= dRange2 ))
										{
											resultFirstRF = 0;
											CheckTFRF = 0;
										}
										else
										{
											resultFirstRF = 1;
											CheckTFRF = 1;
										}
										endCondition = 0; //stop analysis
										end = 0;

									}
									else 
									{
										if((TimingofFirstRF >= dRange1) && (TimingofFirstRF <= dRange2 ))
										{
											resultFirstRF = 0;
											CheckTFRF = 0;
										}
										else
										{
											resultFirstRF = 1;
											CheckTFRF = 1;
										}
										endCondition = 0; //stop analysis
										end = 0;
									}

								}

							}
							else fprintf(file_handle, "Invalid Function Code (%s)  found in line %d\n",_FunctionCode, i);

						}

					}
					if(strcmp(CKSGood, "0") == 0)
					{
						CKSGoodinvalid++ ;
						zerofound = 1;
						fprintf(file_handle, "CKSGood: 0 was found in line %d\n", i);
					}


					//we stop the loop when we are to the end of the test
					sprintf(rangeA, "A%d", i+1);
					ExcelRpt_GetCellValue (worksheetHandle9, rangeA, CAVT_CSTRING, _ATimeColumn);
					
					
					if(strcmp("", _ATimeColumn) == 0)
					{
						endCondition = 0; //stop analysis
						end = 0;
						noTime = 1;
						CheckTFRF = 1;
						resultFirstRF = 1;
						fprintf(file_handle, "No RF signal found!");
					}

					}
				}
				if(zerofound == 0)
				{

				
						if (noTime!=1)
						fprintf(file_handle, "\nTiming %f\n", TimingofFirstRF);
					   
				}

						if(zerofound == 1)
						{
							float percentageCKSGood = ((double)CKSGoodinvalid/( (double)CKSGoodinvalid+ (double)CKSGoodvalid))*100.0;
							fprintf(file_handle, "\n%.2f%% of the CKSGood are equal to 0!\n\n",percentageCKSGood );
						}
					
			

			}
		}

		end=1;
		//return resultFirstRF;
	}
	else
	{
		fprintf(file_handle, "The CheckTimingFirstRF was not performed!\n\n");
		CheckTFRF = -1;
	}
	//printf("check = %d\n",CheckTFRF);
	printCheckResults (file_handle, "CheckTimingFirstRF", CheckTFRF, 0, 0, 0, 0,0,0.0,0.0,0.0);
	return CheckTFRF;

}

//***********************************************************************************************
// Modif - Carolina 06/2019
// Check number of bursts
// return 1 if nb of burst > value
//***********************************************************************************************
int CheckNbBursts(char * wuID, int realValue, char *FCParam, char *FunctionCode, char* InterfacetoCheck)
{

	int boucle = 0, end = 1, endCondition = 1, zerofound = 0;
	char CheckGoodsum[15] = 	"/ROW/@";
	char CheckRTime[15] = 		"/ROW/@";
	char CKSGood[3] = "";
	char _ATimeColumn[255] = "";
	char _RTimeColumn[255] = "";
	char rangeA[30] = "";
	char rangeTime[30]="";
	char GoodSumRange[30] = "";
	char foundCellGoodSumRange[30] = "";
	char cellTimeRange[30]="";
	char cellGoodSumRange[30] = "";
	char Rowparam2[255] = 		"/ROW/@";  //for FC
	char FCRow[30] = "";
	char RTime[30] = "";
	char cellFCRange[30] = "";
	char *CheckParamFC;
	char rangeFC[30] = "";
	char _FunctionCode[50] = "";
	double dtime = 0.0, auxtime = 0.0, sub = 0.0;
	
	int CheckNB=0;
	int countNbBurst = 0;
	int CKSGoodvalid = 0;
	int CKSGoodinvalid = 0;

	char CheckWUID[30] = 		"/ROW/@";
	char _WUIDColumn[255] = "";
	char WUIDRange[30] = "";
	char cellWUIDRange[30] = "";
	char WUID[30] = "";
	int noLab=0;
	
	char CheckDesc[15] = 	"/ROW/@Desc";
	char RFProt[30]="";
	char cellRFProtocol[30]="";
	char RFprotocol[255]="";
	char RFrange[30]="";

	strcat(CheckRTime,translateData(InterfacetoCheck, "Time")); 
	strcat(CheckGoodsum,translateData(InterfacetoCheck, "CKS_Validity"));
	strcat(CheckWUID,translateData(InterfacetoCheck,"ID"));
	//printf("CheckWUID = %s\n",CheckWUID);
	//printf("CheckRTime = %s\n",CheckRTime);
	//printf("CheckGoodsum = %s\n",CheckGoodsum); 
	
	

	
	
	fprintf(file_handle, "\n***************CheckNbBursts***************\n");
	fprintf(file_handle, "                 Check_%d\n\n", NBTestCheck);

	CheckParamFC = strcat(Rowparam2, FCParam); //EX: /ROW/@Function_code
	//Worksheet Logs
	ExcelRpt_GetWorksheetFromIndex(workbookHandle9, 1, &worksheetHandle9);

		//3 - looking for "/ROW/@Desc" 
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckDesc, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,RFProt);
	char caracterDesc[] = "$";
	char *tokenDesc = strtok(RFProt, caracterDesc);
	if(tokenDesc == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckDesc);
	else
		strcpy(cellRFProtocol, tokenDesc);
	
	// 1 - looking for "/ROW/@CKSGood"
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckGoodsum, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,foundCellGoodSumRange);
	char caracter1[] = "$";  //foundCellRange =  $O$2
	char *token1 = strtok(foundCellGoodSumRange, caracter1);   //token1 = O
	if(token1 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckGoodsum);
	else
		strcpy(cellGoodSumRange, token1);

	//2 - looking for "/ROW/@Function_code" = $W$2
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckParamFC, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,FCRow);
	char caracter2[] = "$";
	char *token2 = strtok(FCRow, caracter2);
	if(token2 == NULL)
		fprintf(file_handle, "The column %s was not found, RFprotocole \"%s\" not received!\n\n", CheckParamFC,InterfacetoCheck);
	else
		strcpy(cellFCRange, token2); // cellFCRange = W2

	//3 - looking for "/ROW/@_RTime" = $B$2
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckRTime, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,RTime);
	char caracter3[] = "$";
	char *token3 = strtok(RTime, caracter3);
	if(token3 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckRTime);
	else
		strcpy(cellTimeRange, token3); // cellFCRange = B2
	
	//4 - looking for "/ROW/@ID" = $W$2 // the WU ID
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckWUID, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,WUID);
	char caracterWUID[] = "$";
	char *tokenWUID = strtok(WUID, caracterWUID);
	if(tokenWUID == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckWUID);
	else
		strcpy(cellWUIDRange, tokenWUID);


	if(token1 != NULL && token2 != NULL && token3 != NULL && tokenWUID != NULL)
	{
		if (lab2 == 0)
		{
			if (lab1 == 0)
			{
			fprintf(file_handle, "Between %s (%d ms) and %s (%d ms)\n", 	label1, Time1, label2, Time2);
			}
			else
			{
			fprintf(file_handle, "Between the beginning and %s (%d ms)\n", label2, Time2);
			}
		}
		else
		{
		   	if (lab1 == 0)
			{
			fprintf(file_handle, "Between %s (%d ms) and the end of the script\n", 	label1, Time1);
			}
			else
			{
			fprintf(file_handle, "Between the beginning and the end of the script\n");
			}
		}
		fprintf(file_handle, "Function code: %s\n",FunctionCode);
		fprintf(file_handle, "Expected Bursts Nb: %d\n\n", 	realValue);


		for(boucle=2; end==1; boucle++)
		{
			//until find label1
			sprintf(rangeA, "A%d", boucle+1);
			ExcelRpt_GetCellValue (worksheetHandle9, rangeA, CAVT_CSTRING, _ATimeColumn);

			//starting check when label1 was found
			if(strcmp(label1, _ATimeColumn) == 0 || lab1==1)
			{
				//analyse until find label 2
				for(int i = boucle+1 ; endCondition == 1; i++)
				{
					
					sprintf(RFrange, "%s%d", cellRFProtocol, i);
					ExcelRpt_GetCellValue (worksheetHandle9, RFrange, CAVT_CSTRING,RFprotocol);
				    
					if (strcmp(InterfacetoCheck,"") == 0 || CheckRFProtocol(InterfacetoCheck,RFprotocol) == 1 )
					{
					
					//1 - check Goodsm first
					sprintf(GoodSumRange,"%s%d",cellGoodSumRange, i); //P3
					ExcelRpt_GetCellValue (worksheetHandle9, GoodSumRange, CAVT_CSTRING, CKSGood);

					if(strcmp(CKSGood, "1") == 0)
					{
						
						// we look for the good WU ID
						sprintf(WUIDRange,"%s%d",cellWUIDRange, i);
						ExcelRpt_GetCellValue (worksheetHandle9, WUIDRange, CAVT_CSTRING, _WUIDColumn);

						if(strcmp(_WUIDColumn, wuID) == 0)
						{

							CKSGoodvalid++;
							//2 - check FC second
							sprintf(rangeFC,"%s%d",cellFCRange, i);
							ExcelRpt_GetCellValue (worksheetHandle9, rangeFC, CAVT_CSTRING, _FunctionCode);

							if(strcmp(_FunctionCode, FunctionCode) == 0)
							{
								//check nb bursts
								//2 - check _RTime
								sprintf(rangeTime,"%s%d",cellTimeRange, i);
								ExcelRpt_GetCellValue (worksheetHandle9, rangeTime, CAVT_CSTRING, _RTimeColumn);
								dtime = atof(_RTimeColumn);

								if(auxtime == 0)
								{
									sub =  dtime -  auxtime;
									auxtime = dtime;

									//countInterBurst++;
									countNbBurst++;
								}
								else
								{
									sub =  dtime -  auxtime;
									auxtime = dtime;

									if(sub > IFandIBthreshold)  //IB > 1s
									{
										//countInterBurst++;
										countNbBurst++;
									}
								}
							}
							else fprintf(file_handle, "Invalid Function Code (%s)  found in line %d\n",_FunctionCode, i);
						}
					}
					if(strcmp(CKSGood, "0") == 0)
					{
						CKSGoodinvalid++ ;
						zerofound = 1;
						fprintf(file_handle, "CKSGood: 0 was found in line %d\n", i);
					}


					sprintf(rangeA, "A%d", i);
					ExcelRpt_GetCellValue (worksheetHandle9, rangeA, CAVT_CSTRING, _ATimeColumn);

					if(strcmp(label2, _ATimeColumn) == 0)   //stop condition check
					{
						if (noLab == 1 || FirstLab==1  || TwoLabs==1)
						{
						if(zerofound == 1)
						{
							float percentageCKSGood = ((double)CKSGoodinvalid/( (double)CKSGoodinvalid+ (double)CKSGoodvalid))*100.0;
							fprintf(file_handle, "\n%.2f%% of the CKSGood are equal to 0!\n\n",percentageCKSGood );
						}


						endCondition = 0; //stop analysis
						end = 0;
						}
						noLab=1;
					}
					}
				}
			}
		}
		end=1;
		//fprintf(file_handle, "\nNumber of Burst: %d\n\n", countInterBurst);
		//return countInterBurst;


		if(countNbBurst == 0)
		{
			CheckNB = 1;
		}

		if(countNbBurst != realValue)
		{
			CheckNB = 1;
		}


	}
	else
	{
		fprintf(file_handle, "The CheckNbBursts was not performed!\n\n");

		CheckNB = -1;
	}

	printCheckResults (file_handle, "CheckNbBursts", CheckNB, 0, 0, 0, countNbBurst, realValue,0.0,0.0,0.0);
	return CheckNB;
}

//***********************************************************************************************
// Modif - Carolina 07/2019
// Check number of frames in a burst
// return 1 if nb of burst > value
//***********************************************************************************************
int CheckNbFramesInBurst(char * wuID, int realValue, char *FCParam, char *FunctionCode, char* InterfacetoCheck)
{
	int boucle = 0, end = 1, endCondition = 1, zerofound = 0;
	char CheckGoodsum[15] = 	"/ROW/@";
	char CheckRTime[15] = 		"/ROW/@";
	char CKSGood[3] = "";
	char _ATimeColumn[255] = "";
	char _RTimeColumn[255] = "";
	char rangeA[30] = "";
	char rangeTime[30]="";
	char GoodSumRange[30] = "";
	char foundCellGoodSumRange[30] = "";
	char cellTimeRange[30]="";
	char cellGoodSumRange[30] = "";
	char Rowparam2[255] = 		"/ROW/@";  //for FC
	char FCRow[30] = "";
	char RTime[30] = "";
	char cellFCRange[30] = "";
	char *CheckParamFC;
	char rangeFC[30] = "";
	char _FunctionCode[50] = "";
	double dtime = 0.0, auxtime = 0.0, sub = 0.0;
	int countInterBurst = 0, counNbofFrames = 0;
	int CheckNF = 0;

	int CKSGoodvalid = 0;
	int CKSGoodinvalid = 0;

	char CheckWUID[30] = 		"/ROW/@"; 
	char _WUIDColumn[255] = "";
	char WUIDRange[30] = ""; 
	char cellWUIDRange[30] = ""; 
	char WUID[30] = ""; 
	int noLab=0;
	
	char CheckDesc[15] = 	"/ROW/@Desc";
	char RFProt[30]="";
	char cellRFProtocol[30]="";
	char RFprotocol[255]="";
	char RFrange[30]="";
	
	strcat(CheckRTime,translateData(InterfacetoCheck, "Time")); 
	strcat(CheckGoodsum,translateData(InterfacetoCheck, "CKS_Validity"));
	strcat(CheckWUID,translateData(InterfacetoCheck,"ID"));
	//printf("CheckWUID = %s\n",CheckWUID);
	//printf("CheckRTime = %s\n",CheckRTime);
	//printf("CheckGoodsum = %s\n",CheckGoodsum); 

	
	
	
	fprintf(file_handle, "\n***************CheckNbFramesInBurst***************\n");
	fprintf(file_handle, "                 Check_%d\n\n", NBTestCheck);

	CheckParamFC = strcat(Rowparam2, FCParam); //EX: /ROW/@Function_code
	//Worksheet Logs
	ExcelRpt_GetWorksheetFromIndex(workbookHandle9, 1, &worksheetHandle9);

	// 1 - looking for "/ROW/@CKSGood"
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckGoodsum, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,foundCellGoodSumRange);
	char caracter1[] = "$";  //foundCellRange =  $O$2
	char *token1 = strtok(foundCellGoodSumRange, caracter1);   //token1 = O
	if(token1 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckGoodsum);
	else
		strcpy(cellGoodSumRange, token1);

	//2 - looking for "/ROW/@Function_code" = $W$2
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckParamFC, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,FCRow);
	char caracter2[] = "$";
	char *token2 = strtok(FCRow, caracter2);
	if(token2 == NULL)
		fprintf(file_handle, "The column %s was not found, RFprotocole \"%s\" not received!\n\n", CheckParamFC,InterfacetoCheck);
	else
		strcpy(cellFCRange, token2); // cellFCRange = W2

	//3 - looking for "/ROW/@_RTime" = $B$2
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckRTime, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,RTime);
	char caracter3[] = "$";
	char *token3 = strtok(RTime, caracter3);
	if(token3 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckRTime);
	else
		strcpy(cellTimeRange, token3); // cellFCRange = B2

	//4 - looking for "/ROW/@ID" = $W$2 // the WU ID
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckWUID, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,WUID);
	char caracterWUID[] = "$";
	char *tokenWUID = strtok(WUID, caracterWUID);
	if(tokenWUID == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckWUID);
	else
		strcpy(cellWUIDRange, tokenWUID);

	if(token1 != NULL && token2 != NULL && token3 != NULL && tokenWUID != NULL)
	{
		if (lab2 == 0)
		{
			if (lab1 == 0)
			{
			fprintf(file_handle, "Between %s (%d ms) and %s (%d ms)\n", 	label1, Time1, label2, Time2);
			}
			else
			{
			fprintf(file_handle, "Between the beginning and %s (%d ms)\n", label2, Time2);
			}			 
		}
		else
		{
		   	if (lab1 == 0)
			{
			fprintf(file_handle, "Between %s (%d ms) and the end of the script\n", 	label1, Time1);
			}
			else
			{
			fprintf(file_handle, "Between the beginning and the end of the script\n");
			}
		}
		fprintf(file_handle, "Function code: %s\n",FunctionCode);
		fprintf(file_handle, "Expected Frames Nb in Bursts: %d\n\n", 	realValue);


		for(boucle=2; end==1; boucle++)
		{
			//until find label1
			sprintf(rangeA, "A%d", boucle+1);
			ExcelRpt_GetCellValue (worksheetHandle9, rangeA, CAVT_CSTRING, _ATimeColumn);

			//starting check when label1 was found
			if(strcmp(label1, _ATimeColumn) == 0 || lab1==1)
			{
				//analyse until find label 2
				for(int i = boucle+1 ; endCondition == 1; i++)
				{
					sprintf(RFrange, "%s%d", cellRFProtocol, i);
					ExcelRpt_GetCellValue (worksheetHandle9, RFrange, CAVT_CSTRING,RFprotocol);
				    
					if (strcmp(InterfacetoCheck,"") == 0 || CheckRFProtocol(InterfacetoCheck,RFprotocol) == 1 )
					{
					//1 - check Goodsm first
					sprintf(GoodSumRange,"%s%d",cellGoodSumRange, i); //P3
					ExcelRpt_GetCellValue (worksheetHandle9, GoodSumRange, CAVT_CSTRING, CKSGood);

					if(strcmp(CKSGood, "1") == 0)
					{

						// we look for the good WU ID
						sprintf(WUIDRange,"%s%d",cellWUIDRange, i);
						ExcelRpt_GetCellValue (worksheetHandle9, WUIDRange, CAVT_CSTRING, _WUIDColumn);

						if(strcmp(_WUIDColumn, wuID) == 0)
						{

							CKSGoodvalid++ ;

							//2 - check FC second
							sprintf(rangeFC,"%s%d",cellFCRange, i);
							ExcelRpt_GetCellValue (worksheetHandle9, rangeFC, CAVT_CSTRING, _FunctionCode);

							if(strcmp(_FunctionCode, FunctionCode) == 0)
							{
								//check nb bursts
								//2 - check _RTime
								sprintf(rangeTime,"%s%d",cellTimeRange, i);
								ExcelRpt_GetCellValue (worksheetHandle9, rangeTime, CAVT_CSTRING, _RTimeColumn);
								dtime = atof(_RTimeColumn);

								if(auxtime == 0 && countInterBurst==0)  //first trame
								{
									sub =  dtime -  auxtime;
									auxtime = dtime;

									countInterBurst++;
									fprintf(file_handle, "\nBurst number: %d\n", countInterBurst); //one
									counNbofFrames++;
									fprintf(file_handle, "\n%f\n", auxtime);
								}
								else
								{
									sub =  dtime -  auxtime;
									auxtime = dtime;

									if(sub < IFandIBthreshold && countInterBurst==1)  //IF < 1s or 2s
									{
										counNbofFrames++;
										fprintf(file_handle, "\n%f\n", auxtime);
									}
									else
									{
										countInterBurst=0;  //end of the burst
										//fprintf(file_handle, "\nBurst number: %d\n", countInterBurst);
									}
								}
							}
							else fprintf(file_handle, "Invalid Function Code (%s)  found in line %d\n",_FunctionCode, i);
						}
					}
					if(strcmp(CKSGood, "0") == 0)
					{
						CKSGoodinvalid++ ;
						zerofound = 1;
						fprintf(file_handle, "CKSGood: 0 was found in line %d\n", i);
					}


					sprintf(rangeA, "A%d", i);
					ExcelRpt_GetCellValue (worksheetHandle9, rangeA, CAVT_CSTRING, _ATimeColumn);

					if(strcmp(label2, _ATimeColumn) == 0)   //stop condition of check
					{
						if (noLab == 1 || FirstLab==1  || TwoLabs==1)
						{
						if(zerofound == 1)
						{
							float percentageCKSGood = ((double)CKSGoodinvalid/( (double)CKSGoodinvalid+ (double)CKSGoodvalid))*100.0;
							fprintf(file_handle, "\n%.2f%% of the CKSGood are equal to 0!\n\n",percentageCKSGood );
						}


						endCondition = 0; //stop ckeck
						end = 0;
						}
						noLab=1;
					}   
					}
				}
				}
		}
		end=1;
		//fprintf(file_handle, "\nNumber of Burst: %d\n\n", countInterBurst);
		//fprintf(file_handle, "\nNumber frames in a Burst: %d\n\n", counNbofFrames);
		//return counNbofFrames;

		if(counNbofFrames == 0)
		{
			CheckNF = 1;
		}

		if(counNbofFrames != realValue)
		{
			CheckNF = 1;
		}


	}
	else
	{
		fprintf(file_handle, "The CheckNbBursts was not performed!\n\n");
		CheckNF =  -1;
	}

	printCheckResults (file_handle, "CheckNbFramesInBurst", CheckNF, 0, 0, counNbofFrames, 0, realValue,0.0,0.0,0.0);
	return CheckNF;


}

//***********************************************************************************************
// Modif - Carolina 07/2019
// Check timing between two bursts
// return 1
//***********************************************************************************************
int CheckTimingInterBursts(char * wuID, char *realValue, char *FCParam, char *FunctionCode, double dtolValue, double dtolpercent, char* InterfacetoCheck)
{
	int boucle = 0, end = 1, endCondition = 1, zerofound = 0, CheckTIB = 0, i=0, valide = 0, notvalide =0;
	char CheckGoodsum[15] = 	"/ROW/@";
	char CheckRTime[15] = 		"/ROW/@";
	char CKSGood[3] = "";
	char _ATimeColumn[255] = "";
	char _RTimeColumn[255] = "";
	char rangeA[30] = "";
	char rangeTime[30]="";
	char GoodSumRange[30] = "";
	char foundCellGoodSumRange[30] = "";
	char cellTimeRange[30]="";
	char cellGoodSumRange[30] = "";
	char Rowparam2[255] = 		"/ROW/@";  //for FC
	char FCRow[30] = "";
	char RTime[30] = "";
	char cellFCRange[30] = "";
	char *CheckParamFC;
	char rangeFC[30] = "";
	char _FunctionCode[50] = "";
	double dtime = 0.0, auxtime = 0.0, sub = 0.0,
		   dValue = 0.0, dValueMax=0.0, dValueMin = 0.0, dValueMaxperc = 0.0, dValueMinperc = 0.0;
	int  countfc = 0;
	int countNbBurst = 0, counNbofFrames = 0;
	char auxVal[30] = "";
	//char stolValue[30] = "";
	//char stolpercent[30] = "";
	char percent[4]="[%]";

	int CKSGoodvalid = 0;
	int CKSGoodinvalid = 0;

	char CheckWUID[30] = 		"/ROW/@";
	char _WUIDColumn[255] = "";
	char WUIDRange[30] = "";
	char cellWUIDRange[30] = "";
	char WUID[30] = "";
	int noLab=0;

	char CheckDesc[15] = 	"/ROW/@Desc";
	char RFProt[30]="";
	char cellRFProtocol[30]="";
	char RFprotocol[255]="";
	char RFrange[30]="";
	

	strcpy(auxVal, realValue); 						 // auxVal = 5;
	dValue = atof(auxVal); 		// dValue = 5
	dValue = dValue*1000;
	
	dtolValue = dtolValue*1000;
	
	strcat(CheckGoodsum,translateData(InterfacetoCheck, "CKS_Validity"));
	strcat(CheckWUID,translateData(InterfacetoCheck,"ID"));
	strcat(CheckRTime,translateData(InterfacetoCheck, "Time"));
	//printf("CheckWUID = %s\n",CheckWUID);
	//printf("CheckRTime = %s\n",CheckRTime);
	//printf("CheckGoodsum = %s\n",CheckGoodsum);
	
	fprintf(file_handle, "\n***************CheckTimingInterBursts***************\n");
	fprintf(file_handle, "                 Check_%d\n\n", NBTestCheck);

	CheckParamFC = strcat(Rowparam2, FCParam); //EX: /ROW/@Function_code
	//Worksheet Logs
	ExcelRpt_GetWorksheetFromIndex(workbookHandle9, 1, &worksheetHandle9);

	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckDesc, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,RFProt);
	char caracterDesc[] = "$";
	char *tokenDesc = strtok(RFProt, caracterDesc);
	if(tokenDesc == NULL)
	{
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckDesc);
		return 1;
	}
	 else
	 	strcpy(cellRFProtocol, tokenDesc);
	
	// 1 - looking for "/ROW/@CKSGood"
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckGoodsum, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,foundCellGoodSumRange);
	char caracter1[] = "$";  //foundCellRange =  $O$2
	char *token1 = strtok(foundCellGoodSumRange, caracter1);   //token1 = O
	if(token1 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckGoodsum);
	else
		strcpy(cellGoodSumRange, token1);

	//2 - looking for "/ROW/@Function_code" = $W$2
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckParamFC, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,FCRow);
	char caracter2[] = "$";
	char *token2 = strtok(FCRow, caracter2);
	if(token2 == NULL)
		fprintf(file_handle, "The column %s was not found, RFprotocole \"%s\" not received!\n\n", CheckParamFC,InterfacetoCheck);
	else
		strcpy(cellFCRange, token2); // cellFCRange = W2

	//3 - looking for "/ROW/@_RTime" = $B$2
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckRTime, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,RTime);
	char caracter3[] = "$";
	char *token3 = strtok(RTime, caracter3);
	if(token3 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckRTime);
	else
		strcpy(cellTimeRange, token3); // cellFCRange = B2

	//4 - looking for "/ROW/@ID" = $W$2 // the WU ID
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckWUID, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,WUID);
	char caracterWUID[] = "$";
	char *tokenWUID = strtok(WUID, caracterWUID);
	if(tokenWUID == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckWUID);
	else
		strcpy(cellWUIDRange, tokenWUID);
	
	
	if(token1 != NULL && token2 != NULL && token3 != NULL && tokenWUID != NULL)
	{
		if (lab2 == 0)
		{
			if (lab1 == 0)
			{
			fprintf(file_handle, "Between %s (%d ms) and %s (%d ms)\n", 	label1, Time1, label2, Time2);
			}
			else
			{
			fprintf(file_handle, "Between the beginning and %s (%d ms)\n", label2, Time2);
			}
		}
		else
		{
		   	if (lab1 == 0)
			{
			fprintf(file_handle, "Between %s (%d ms) and the end of the script\n", 	label1, Time1);
			}
			else
			{
			fprintf(file_handle, "Between the beginning and the end of the script\n");
			}
		}
		fprintf(file_handle, "Function code: %s\n",	FunctionCode);
		fprintf(file_handle, "Expected IB timing: %s sec\n",				realValue);
		fprintf(file_handle, "Tolerance value: %f ms\n", 	dtolValue);
		fprintf(file_handle, "Tolerance %s: %f\n\n",	percent, dtolpercent);




		for(boucle=2; end==1; boucle++)
		{
			//until find label1
			sprintf(rangeA, "A%d", boucle+1);
			ExcelRpt_GetCellValue (worksheetHandle9, rangeA, CAVT_CSTRING, _ATimeColumn);
			//printf("label1 not found");
			//starting check when label1 was found
		//	printf("label1 = %s and _ATimeColumn = %s\n",label1,_ATimeColumn);
			if(strcmp(label1, _ATimeColumn) == 0 || lab1==1)
			{
				//analyse until find label 2
				for(i = boucle+1 ; endCondition == 1; i++)
				{
					sprintf(RFrange, "%s%d", cellRFProtocol, i);
					ExcelRpt_GetCellValue (worksheetHandle9, RFrange, CAVT_CSTRING,RFprotocol);
				    
					if (strcmp(InterfacetoCheck,"") == 0 || CheckRFProtocol(InterfacetoCheck,RFprotocol) == 1 )
					{
					
					//1 - check Goodsm first
					sprintf(GoodSumRange,"%s%d",cellGoodSumRange, i); //P3
					//printf("Cell num = %s%d\n",cellGoodSumRange, i);
					ExcelRpt_GetCellValue (worksheetHandle9, GoodSumRange, CAVT_CSTRING, CKSGood);
					//printf("CKSgood = %s\n",CKSGood);
					if(strcmp(CKSGood, "1") == 0)
					{

						// we look for the good WU ID
						sprintf(WUIDRange,"%s%d",cellWUIDRange, i);
						ExcelRpt_GetCellValue (worksheetHandle9, WUIDRange, CAVT_CSTRING, _WUIDColumn);

						if(strcmp(_WUIDColumn, wuID) == 0)
						{


							CKSGoodvalid++ ;

							//2 - check FC second
							sprintf(rangeFC,"%s%d",cellFCRange, i);
							ExcelRpt_GetCellValue (worksheetHandle9, rangeFC, CAVT_CSTRING, _FunctionCode);

							if(strcmp(_FunctionCode, FunctionCode) == 0)
							{
								//3 - check _RTime
								sprintf(rangeTime,"%s%d",cellTimeRange, i);
								ExcelRpt_GetCellValue (worksheetHandle9, rangeTime, CAVT_CSTRING, _RTimeColumn);
								dtime = atof(_RTimeColumn);

								if(auxtime == 0)  //first emission
								{
									sub =  dtime -  auxtime;
									auxtime = dtime;
									countNbBurst++;
									counNbofFrames++;

									//fprintf(file_handle, "\n%d\n\n", countInterBurst);
								}
								else
								{
									sub =  dtime -  auxtime;
									auxtime = dtime;

									counNbofFrames++;

									if(sub > IFandIBthreshold)  //IB > 1s
									{

										countNbBurst++;
										//fprintf(file_handle, "\nsub %f\n", sub);

										if(dtolValue != 0 && dtolpercent == 0)
										{
											dValueMax = dValue + dtolValue;
											dValueMin = dValue - dtolValue;
											if(dValueMin<0)
												dValueMin = 0;

											if(sub >= dValueMin && sub <= dValueMax )
											{
												valide++;
												//fprintf(file_handle, "The TimingInterBurst %f is between [%f,%f]\n", sub, dValueMin, dValueMax);
												//fprintf(file_handle, "\ncountInterBurst %d\n", countInterBurst);
											}
											else
											{
												CheckTIB = 1;
												notvalide++;
												fprintf(file_handle, "IB timing %f is not between [%f,%f], (frames at %f and %f)\n", sub, dValueMin, dValueMax,(dtime-sub),dtime);
												//fprintf(file_handle, "\nsub %f\n", sub);
											}
										}
										else if(dtolValue == 0 && dtolpercent != 0)
										{
											dValueMaxperc = dValue + (dValue*(dtolpercent));
											dValueMinperc = dValue - (dValue*(dtolpercent));
											if(dValueMinperc<0)
												dValueMinperc = 0;

											if(sub >= dValueMinperc && sub <= dValueMaxperc)
											{
												//fprintf(file_handle, "The TimingInterBurst %f is between [%f,%f]\n", sub, dValueMinperc, dValueMaxperc);
												valide++;
											}
											else
											{
												notvalide++;
												CheckTIB = 1;
												fprintf(file_handle, "IB timing %f is not between [%f,%f], (frames at %f and %f)\n", sub, dValueMinperc, dValueMaxperc,(dtime-sub),dtime);
											}
										}
										else 
										{
											if(dValue<0)
											   dValue = 0;

											if(sub >= dValue && sub <= dValue)
											{
												//fprintf(file_handle, "The TimingInterBurst %f is between [%f,%f]\n", sub, dValueMinperc, dValueMaxperc);
												valide++;
											}
											else
											{
												notvalide++;
												CheckTIB = 1;
												fprintf(file_handle, "IB timing %f is not equal to [%f], (frames at %f and %f)\n", sub, dValue,(dtime-sub),dtime);
											}	
										}

									}
								}

							}
							else
							{
								countfc++; //wrong fc
								fprintf(file_handle, "Invalid Function Code (%s)  found in line %d\n",_FunctionCode, i);
							}

						}
					}

					/*else{
						countfc++; //wrong fc
					}   */

					//	}
					if(strcmp(CKSGood, "0") == 0)
					{

						CKSGoodinvalid++ ;
						zerofound = 1;
						fprintf(file_handle, "CKSGood: 0 was found in line %d\n", i);
					}


					sprintf(rangeA, "A%d", i);
					ExcelRpt_GetCellValue (worksheetHandle9, rangeA, CAVT_CSTRING, _ATimeColumn);

					//printf("label2 = _%s_ and TimeColumn = _%s_\n",label2, _ATimeColumn);
					if(strcmp(label2, _ATimeColumn) == 0)   //stop condition of check
					{
						//printf("HEREEE\n");
						if (noLab == 1 || FirstLab==1  || TwoLabs==1)
						{
						if(zerofound == 1)
						{
							float percentageCKSGood = ((double)CKSGoodinvalid/( (double)CKSGoodinvalid+ (double)CKSGoodvalid))*100.0;
							fprintf(file_handle, "\n%.2f%% of the CKSGood are equal to 0!\n\n",percentageCKSGood );
						}


						endCondition = 0; //stop ckeck
						end = 0;
						}
						noLab=1;
					}
				}
				}
			}
		}
		if(countfc == i)
		{
			CheckTIB = 1;
		}
		end=1;
		//fprintf(file_handle, "\nNumber of bursts %d\n", countInterBurst);
		if(countNbBurst  == 0)
		{
			CheckTIB =1;
		}

		//Ajout MaximePAGES 07/07/2020


		//return CheckTIB;
	}
	else
	{
		fprintf(file_handle, "The CheckTimingInterBursts was not performed!\n\n");
		CheckTIB = -1;

	}


	printCheckResults (file_handle, "CheckTimingInterBursts", CheckTIB, valide, notvalide, counNbofFrames , countNbBurst , 0,0.0,0.0,0.0);
	return CheckTIB;
}

//***********************************************************************************************
// Modif - Carolina 06/2019
// CLOSE applications Excel
//***********************************************************************************************
void closeApplicationsExcel()
{
	//ExcelRpt_WorkbookClose (workbookHandledata, 0);
	//ExcelRpt_ApplicationQuit(applicationHandledata);

//other
	//ExcelRpt_WorkbookClose (workbookHandle, 0);
	//ExcelRpt_ApplicationQuit(applicationHandle);

	//ExcelRpt_WorkbookClose (workbookHandle8, 0);
	//ExcelRpt_ApplicationQuit(applicationHandle8);

	//Logs.xlsx
	//ExcelRpt_WorkbookClose (workbookHandle9, 0);
	//ExcelRpt_ApplicationQuit(applicationHandle9);

	//ExcelRpt_WorkbookClose (workbookHandle4, 0);
	//ExcelRpt_ApplicationQuit(applicationHandle4);

	//ExcelRpt_ApplicationQuit(applicationLabel);



	//CA_DiscardObjHandle(applicationHandle);

	//CA_DiscardObjHandle(applicationHandlesave);
	//CA_DiscardObjHandle(applicationHandledata);
	//CA_DiscardObjHandle(applicationLabel);
	//CA_DiscardObjHandle(applicationHandle4);
	//CA_DiscardObjHandle(applicationHandle7);

	//Fermeture excel
	ExcelRpt_WorkbookClose (workbookHandleCurrentScript, 0);
	//ExcelRpt_ApplicationQuit (applicationHandleCurrentScript);
	//CA_DiscardObjHandle(applicationHandleCurrentScript);

}

//***********************************************************************************************
// Modif - Carolina 04/2019
// Opening the current expected results to start the analysis
//***********************************************************************************************
void openingExpectedResults(char *testScriptFile, char *sProjectDir, char * sDirForAnalyseOnly)
{
	Point cell;

	int EndBoucle = 0, EndBoucleTS = 0, nb = 0;
	int boucle = 0, j=0, Time=0, month, day, year;
	int index=0; 
	//MODIF MaximePAGES 29/06/2020 E200
	int *tabtoken[50];// = {NULL};
	//int oneloop = 1;
	//int ** tabtoken = NULL;
	//****************

	
	int iResultSTDEV = 0, CheckFieldValueOK = 0, LabelRF = 0, iNbofBurst=0, iNoRF = 0, iFirstRF = 0, NbofFrames = 0;
	int iTimingInterBursts = 0, iTimingInterFrames = 0;
	int iCompareP = 0,  iCompareAcc = 0;
	char range[30];
	char CommandCheck[255] = "";
	char realParam[255] = "";
	char Labels[255] = "";
	char Parametertocheck[255]= "";
	char Value[255] = "";
	char Tolerancepercent[255] = "";
	char Tolerance[255]= "";
	char timeTS[255]="";
	char labelTS[255]="";
	char InterfacetoCheck[255]="";
	char *realParametertoCheck="";
	char CurrentPath[MAX_PATHNAME_LEN];
	char pathname[MAX_PATHNAME_LEN];
	//char realValue[255] = "";
	char FCParam[10] = "WU_State";
	char Param[50] ="";
	char *translatedFCParam="";
	char realFCParam[255] = "";
	char FunctionCodeValue[255] = "";
	char sTranslateFrameNumber[100]="";
	char passTranslatedFrameNumber[100] = "";
	char sTranslateAngleP[100]="";
	char passTranslatedAngleP[100]="";
	
	char wuID[25] = "";
	//char realRange1[255] ="";
	//char realRange2[255] = "";
	char auxVal[255] = "";
	//char *tabVal[20];
	//char *tVal[20];
	double dValue = 0.0, tol1_1=0.0, tol2_2=0.0; // tol1=0.0, tol2=0.0;
	int errOneLabel = 0, errTwoLabel = 0;
	int find =0;
	int find2 = 0;

	strcpy(CurrentPath, sProjectDir);
	strcpy(fichierLogxml, sProjectDir); //Ex d:\...\Test_Analyse\TestCheckSTDEVCAngle_workbook9 23-05-2019 10-52-31
	strcat(fichierLogxml, "\\Logs.xml"); //Ex d:\...\Test_Analyse\TestCheckSTDEVCAngle_workbook9 23-05-2019 10-52-31\Logs.xml

	closeApplicationsExcel();

	//ExcelRpt_ApplicationNew(VFALSE,&applicationHandle1);   //translate
	//ExcelRpt_ApplicationNew(VFALSE,&applicationHandle7);

	//create a handle for this file
	//ExcelRpt_ApplicationNew(VFALSE,&applicationLabel);  //Logs.xml
	ExcelRpt_WorkbookOpen (applicationHandleProject, fichierLogxml, &workbookHandle9); //open xml with Excel
	//ExcelRpt_GetWorksheetFromIndex(workbookHandle9, 1, &worksheetHandle9);

	
	
	if (ongoingAnalyse == 1) 
	{
		//create a file report
		strcpy(pathname, sDirForAnalyseOnly);
		GetSystemDate(&month, &day, &year);
		MakePathname(pathname,"Check_Results.txt" , pathname);
		
		file_handle = fopen (pathname, "a+");  
	}
	else 
	{
		//create a file report
		strcpy(pathname, sProjectDir);
		GetSystemDate(&month, &day, &year);
		MakePathname(pathname,"Check_Results.txt" , pathname);
		
		file_handle = fopen (pathname, "w+");  
	}
	
	
	if(file_handle == NULL)
	{
		fprintf(file_handle,"The file couldn't be created\n");
		return;
	}

	fprintf(file_handle, "\n%s\n", fichierLogxml);
	fprintf(file_handle, "\nCheck Report %d-%d-%d\n\n", day, month, year);

	//opening excel to read functions
//MODIF MaximePAGES 18/06/2020
	ExcelRpt_WorkbookOpen (applicationHandleProject, testScriptFile, &workbookHandle4);
	ExcelRpt_GetWorksheetFromIndex(workbookHandle4, 2, &worksheetHandle4);
	ExcelRpt_GetCellValue (worksheetHandle4, "I1", CAVT_INT,&EndBoucle); //EndBoucle = Number of lines in the script

	for(boucle=1; boucle <= EndBoucle-1; boucle++)
	{

		
		sprintf(range,"A%d",boucle+1);
		ExcelRpt_GetCellValue (worksheetHandle4, range , CAVT_CSTRING, CommandCheck);
		if(strcmp(CommandCheck, "CheckTimingFirstRF") == 0 )
		{
			LabelRF = 1;
		}

		sprintf(range,"B%d",boucle+1);
		ExcelRpt_GetCellValue (worksheetHandle4,range , CAVT_CSTRING,FunctionCodeValue);

		sprintf(range,"C%d",boucle+1);
		ExcelRpt_GetCellValue (worksheetHandle4, range , CAVT_CSTRING,Labels);

		if(LabelRF == 0)
		{
			//recuperer label
			RetrieveandSeparateLabel(Labels);
		}
		else
		{
			char caracter[] = ";";
			strtok(Labels, caracter);   //Label_label1;
			strcpy(label1, Labels);  	//Label_label1
			strcpy(label2, "");
		}

		sprintf(range,"D%d",boucle+1);
		ExcelRpt_GetCellValue (worksheetHandle4, range , CAVT_CSTRING,InterfacetoCheck);

		if(strcmp(InterfacetoCheck, "") != 0)
		{

			translatedFCParam = translateData(InterfacetoCheck, FCParam); 	// FCParam = WU_State (always)
			strcpy(realFCParam, translatedFCParam); 						// Ex: realFCParam =  Function_code -> if Stand_Frm interface
		}
		else
		{
			strcpy(realFCParam,StandardToAttribute(FCParam));
		}

		//parameter to check
		sprintf(range,"E%d",boucle+1);
		ExcelRpt_GetCellValue (worksheetHandle4, range , CAVT_CSTRING,Parametertocheck);

		if(strcmp(Parametertocheck, "") != 0)
		{
			//Ex: if interface = Stand_Frm and parametertocheck id WU_State , into the .xml WU_State is Function_code
			realParametertoCheck = translateData(InterfacetoCheck, Parametertocheck);
			strcpy(realParam, realParametertoCheck); 						//realParam = realParametertoCheck;
		}

		sprintf(range,"F%d",boucle+1);
		ExcelRpt_GetCellValue (worksheetHandle4, range , CAVT_CSTRING,Value); //depends of the type (sequence, range ...)
		strcpy(auxVal, Value);		  //5;

		sprintf(range,"G%d",boucle+1);
		ExcelRpt_GetCellValue (worksheetHandle4, range , CAVT_CSTRING,Tolerance);
		tol1_1 = atof(Tolerance); //traslateParameterValue(Tolerance); //Modif MaximePAGES 06/08/2020
		//tol1_1 = (tol1_1)*1000.0;

		sprintf(range,"H%d",boucle+1);
		ExcelRpt_GetCellValue (worksheetHandle4, range , CAVT_CSTRING,Tolerancepercent);
		tol2_2 = atof(Tolerancepercent); //traslateParameterValue(Tolerance); //Modif MaximePAGES 06/08/2020
		tol2_2 = (tol2_2)/100.0;

		//Take time from "TestCase" (workbookHandle4 - worksheetHandle3)
		ExcelRpt_GetWorksheetFromIndex(workbookHandle4, 1, &worksheetHandle3);
		ExcelRpt_GetCellValue (worksheetHandle3, "I1", CAVT_INT,&EndBoucleTS); //EndBoucleTS - end boucle TestScript

		//MODIF MaximePAGES 29/06/2020 E200
		//if (oneloop == 1) {  // we want to initialized the tab just one time
		//	tabtoken = malloc(EndBoucleTS * sizeof(int*));  //dynamic memory allocation
		//	oneloop = 0;
		//}
		//****************

		for(int i = 1; i <= EndBoucleTS-1; i++)
		{
			//compare labels
			sprintf(range,"C%d",i+1);
			ExcelRpt_GetCellValue (worksheetHandle3, range ,CAVT_CSTRING, labelTS);  //
			if(LabelRF == 0)
			{
				int a = strcmp(labelTS, label1);
				int b = strcmp(labelTS, label2);
				//printf("\n\n\nLABELTS is __%s__\n\n\n",labelTS);
				if((a == 0) || (b == 0))
				{
					cell.x = 1;
					sprintf(range,"A%d",i+1);
					ExcelRpt_GetCellValue (worksheetHandle3,range , CAVT_CSTRING, timeTS);

					//int valaux = 0;
					//tabtoken[j]=&valaux;

					//Time = atoi(timeTS);
					//Time = Time * 1000;

					//*tabtoken[j] = Time;
					//Time1 = *tabtoken[0];
					//Time2 = *tabtoken[j];
					//j++;


					//Modif MaximePAGES 29/07/2020 - parameters implementation
					//Time = (int)parameterToValue(timeTS);
					Time = atoi(parameterToValue_Str(timeTS)); //Modif MaximePAGES 05/08/2020
					Time = Time * 1000;
					tabtoken[j] = Time;
					Time1 = tabtoken[0];
					//printf("TIME11 = %d\n",Time1);
					Time2 = tabtoken[j];
					//printf("TIME21 = %d\n",Time2);
					if (lab1 == 1)
					{
					Time1 = 0;
					}
					if (lab2 == 1)
					{
					Time2=0;
					}
					
					j++;
					}
			}
			else
			{
				//int c = strcmp(labelTS, label1);
				if((strcmp(labelTS, label1) == 0) && (strcmp(label2, "") == 0))
				{
					cell.x = 1;
					sprintf(range,"A%d",i+1);
					ExcelRpt_GetCellValue (worksheetHandle3,range , CAVT_CSTRING, timeTS);
					//Time = atoi(timeTS);
					//Time = Time * 1000;
					//tabtoken[j] = &Time;
					//Time1 = *tabtoken[0];
					//Time2=0;
					Time = atoi(timeTS);
					Time = Time * 1000;
					tabtoken[j] = Time;
					Time1 = tabtoken[0];
					Time2=0;
				}
			}

		}
	
		if (index !=0)
		{
		//printf("tabhistry = __%s__\n",labelHistory[index]);
		find = labelAlreadyInsert(label1);
		find2 = labelAlreadyInsert(label2);
		}
		if (strcmp(label1,"")!=0)
		{
		strcpy(labelHistory[index],label1);
		index++;
	    }
		if (strcmp(label2,"") != 0)
		{ 
		strcpy(labelHistory[index],label2);
		index++;
		}
		
		//printf("find = %d\nfind2= %d\n",find,find2);
		if (find == 0 && find2 == 0 )
		{
			//printf("LabelRF = %d\nlab1 = %d\nlab2 = %d\n",LabelRF,lab1,lab2);
			if(LabelRF == 0 && lab1 ==0 && lab2==0)
			{
				errTwoLabel = InsertTwoLabel(); 
			}
			else if ((LabelRF == 1) || (lab1 ==0 && lab2 == 1))
			{
				//Time2=0;
				errOneLabel = InsertOneLabel();
				LabelRF = 0;
			}
			else if ( LabelRF == 0 && lab1 ==1 && lab2 ==0)
			{
				//Time1 = 0 ;
				errOneLabel = InsertOneLabel2();
			}
		}
		else if (find == 1 && find2 == 0)
		{	
			//Time1 =0;
			errOneLabel = InsertOneLabel2();
		}
		else if (find == 0 && find2 == 1)
		{
			//Time2=0;
			errOneLabel = InsertOneLabel();
			LabelRF = 0;
		}
	  
		// Modif MaximePAGES 14/08/2020 - WU ID modification (test for each WU ID)
		for (int id = 0; id < 2; id++)
		{
			//Modif MaximePAGES - 26/08/2020 Stand and Diag Frame management 
			
			if (strcmp(InterfacetoCheck,"Stand_Frm") == 0) 
			{
			   strcpy(wuID,tabStandWUID[id]);  
			}
			
			if (strcmp(InterfacetoCheck,"Diag_Frm") == 0) 
			{
			   strcpy(wuID,tabDiagWUID[id]);  
			}
			
			if (strcmp(InterfacetoCheck,"SW_Ident") == 0) 
			{
			   strcpy(wuID,tabSWIdentWUID[id]);  
			}
			
			if (strcmp(InterfacetoCheck,"") == 0) 
			{
			   strcpy(wuID,tabStandWUID[id]);  
			}
			
			
			fprintf(file_handle, "\n||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||\n"); 
			fprintf(file_handle, "|| TEST FOR WU ID \"%s\" |||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||\n", wuID);
			fprintf(file_handle, "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||\n\n");  
			
			//******************************Check functions*****************************************//
			if(errOneLabel == 0 || errTwoLabel == 0)
			{
				if(strcmp(CommandCheck, "CheckTimingInterFrames") == 0)
				{
					iTimingInterFrames =  CheckTimingInterFrames(wuID, auxVal, realFCParam, FunctionCodeValue, tol1_1, tol2_2, InterfacetoCheck);

					/*
					if(iTimingInterFrames == 1 || iTimingInterFrames == -1)
					{
						iCheckTimingInterFrames = 1;
						contErr++;
						fprintf(file_handle, "\CheckTimingInterFrames ... failed!\n\n");
					}
					else
					{
						fprintf(file_handle, "\CheckTimingInterFrames ... passed!\n\n");
					}
					*/

					j=0;
					iTimingInterFrames = 0;
				}

				if(strcmp(CommandCheck, "CheckTimingInterBursts") == 0 )
				{
					iTimingInterBursts = CheckTimingInterBursts(wuID, auxVal, realFCParam, FunctionCodeValue, tol1_1, tol2_2, InterfacetoCheck);

			
					j=0;
					iTimingInterBursts = 0;

				}

				if(strcmp(CommandCheck, "CheckFieldValue") == 0 )
				{

					CheckFieldValueOK = CheckFieldValue(wuID, realParam, auxVal, tol1_1, tol2_2, InterfacetoCheck);

					/*
					if(CheckFieldValueOK == 1 || CheckFieldValueOK == -1)
					{
						iResultCheckFieldValue = 1; //iResultCheckFieldValue global variable
						contErr++;
					}  */
					j=0;
					CheckFieldValueOK = 0;
				}
				if(strcmp(CommandCheck, "CheckSTDEV") == 0 )
				{

					//raz tab
					for (int i = 0 ; i < 100 ; i++)
					{
						strcpy(myFrameTab[i].angleval,"");
						strcpy(myFrameTab[i].anglepos,""); 
					}
					



					//Ex: sTranslateFrameNumber = "1" //Number of the first frame 
					sprintf(sTranslateFrameNumber,"%d",TranslateFrameNb()); 
					strcpy(passTranslatedFrameNumber, sTranslateFrameNumber);
					
					//Ex: sTranslateAngleP = "4"  //Number of angle position 
					sprintf(sTranslateAngleP,"%d",TranslateAnglePosition());
					strcpy(passTranslatedAngleP, sTranslateAngleP);
					
					int iCheckPreSTDEV = 0;
					iCheckPreSTDEV = CheckPreSTDEV(wuID, auxVal, passTranslatedFrameNumber, passTranslatedAngleP, realFCParam, FunctionCodeValue, realParam,InterfacetoCheck);
					
					int iCheckIndivSTDEV = 0;
					iCheckIndivSTDEV = CheckIndivSTDEV();
					
					//iaverage = CheckAverage(wuID, passTranslatedFrameNumber, passTranslatedAngleP, realFCParam, FunctionCodeValue, realParam);
					
					iResultSTDEV = CheckNewSTDEV(passTranslatedAngleP,iCheckPreSTDEV,iCheckIndivSTDEV);
					
					/*
					if(iaverage >= 0)
					{
						iResultSTDEV = CheckSTDEV(wuID, auxVal, iaverage, passTranslatedFrameNumber, passTranslatedAngleP, realFCParam, FunctionCodeValue, realParam);
					}
					else
						fprintf(file_handle, "The CheckSTDEV was not performed!\nError in CheckAverage\n\n");
					*/
					
					
					//raz tab
					
					for (int j = 0 ; j < 100 ; j++)
					{
						strcpy(myFrameTab[j].angleval,"");
						strcpy(myFrameTab[j].anglepos,""); 
					}
					
					/*
					if(iResultSTDEV == 1 || iResultSTDEV == -1)
					{
						iResultCheckSTDEV = 1; 										//iResultCheckSTDEV global variable
						contErr++;
					}
					
					*/
					j=0; //reinitialization of tabtoken
					iResultSTDEV = 0;
				}

				if(strcmp(CommandCheck, "CheckNbBursts") == 0 )
				{
					// auxVal = "0-4;" or "105;" or "105;117;129;"
					nb = conta(auxVal);

					if(nb == 1)    //  105;
					{
						dValue = atof(auxVal);
					}

					iNbofBurst = CheckNbBursts(wuID, dValue, realFCParam, FunctionCodeValue, InterfacetoCheck);


					/*
					if(iNbofBurst == dValue)
					{
						fprintf(file_handle, "\nCheckNbBursts ... passed!\n\n");
					}
					else // if(iNbofBurst < dValue) or  if(iNbofBurst > dValue || iNbofBurst == -1 || iNbofBurst == 0)
					{
						iCheckNbofBurst = 1;
						contErr++;
						fprintf(file_handle, "\nCheckNbBursts ... failed!\n\n");
					} */

					j=0;
					iNbofBurst=0;
				}

				if(strcmp(CommandCheck, "CheckNbFramesInBurst") == 0 )
				{
					nb = conta(auxVal);

					if(nb == 1)    //  105;
					{
						dValue = atof(auxVal);
					}

					NbofFrames = CheckNbFramesInBurst(wuID, dValue,realFCParam, FunctionCodeValue, InterfacetoCheck);

					/*
					if(NbofFrames == dValue)	   // NbofFrames == 4
					{
						fprintf(file_handle, "\nCheckNbFramesInBurst ... passed!\n\n");
					}
					else  // if (NbofFrames > dValue || NbofFrames == -1 || NbofFrames == 0)
					{
						iCheckNbFramesInBurst = 1;
						contErr++;
						fprintf(file_handle, "\nCheckNbFramesInBurst ... failed!\n\n");
					}   */


					j=0;
					NbofFrames=0;
				}




				if(strcmp(CommandCheck, "CheckNoRF") == 0 )
				{
					iNoRF = CheckNoRF(wuID, InterfacetoCheck);

					/*
					if(iNoRF == 1 || iNoRF == -1)
					{
						iCheckNoRF = 1;
						contErr++;
						fprintf(file_handle, "\nCheckNbFramesInBurst ... failed!\n\n");
					}
					else
					{
						fprintf(file_handle, "\nCheckNbFramesInBurst ... passed!\n\n");
					}

					*/
					j=0;
					iNoRF=0;
				}

				if(strcmp(CommandCheck, "CheckTimingFirstRF") == 0 )
				{
				
					iFirstRF = CheckTimingFirstRF(wuID, auxVal, realFCParam, FunctionCodeValue, tol1_1, tol2_2, InterfacetoCheck);

					/*
					if(iFirstRF == 1 || iFirstRF == -1)
					{
						iCheckTimingFirstRF = 1;
						contErr++;
						fprintf(file_handle, "\nCheckNbFramesInBurst ... failed!\n\n");
					}
					else
					{
						fprintf(file_handle, "\nCheckNbFramesInBurst ... passed!\n\n");
					}
					*/
					j=0;
					iFirstRF=0;
				}


				if(strcmp(CommandCheck, "CheckCompareP") == 0 )
				{

					iCompareP = CheckCompareP(wuID, tol1_1, tol2_2,realParam, InterfacetoCheck);
					j=0;
					iCompareP = 0;
				}


				if(strcmp(CommandCheck, "CheckCompareAcc") == 0 )
				{
					iCompareAcc = CheckCompareAcc(wuID,auxVal, tol1_1, tol2_2,realParam, InterfacetoCheck);
					j=0;
					iCompareAcc = 0;
				}




				errOneLabel=0;
				errTwoLabel=0;
			}

		}
		if(errOneLabel == -1 || errTwoLabel == -1)
		{
			fprintf(file_handle, "\nError Label!\n\n");

			ExcelRpt_WorkbookClose (workbookHandle9, 0);
			//ExcelRpt_ApplicationQuit (applicationLabel);
		}
		/*	if(errOneLabel == -1 || errTwoLabel == -1){
				fprintf(file_handle, "\nError Label!\n\n");

				//ExcelRpt_WorkbookClose (workbookHandle9, 0);
				//ExcelRpt_ApplicationQuit (applicationLabel);*/

		strcpy(label1,""); 
		strcpy(label2,""); 
		Time1 = 0;
		Time2 = 0;
		strcpy(range1,"");
		strcpy(range2,""); 

	}

	
	
	
	fclose (file_handle);
	//final save
	char filelog[30] = "";
	

	if (ongoingAnalyse == 1)  //ANALYSE MODE
	{
		
	
		int len = 0;
		char line[1000];
		FILE *file;
		file = fopen(pathname, "r");
		if (file)
		{
			
			while (fgets(line, 1000, file) != NULL) 
			{
				len = strlen(line);
				line[len-1] ='\0';
				InsertTextBoxLine (GiPanel, PANEL_MODE_TEXTBOX_ANALYSIS,-1,line); 
			}
			
			fclose(file);
		}



		sprintf(filelog, "\\Logs.xlsx");
		strcpy(fichierLogxlsx, sDirForAnalyseOnly);
		strcat(fichierLogxlsx,filelog);
		ExcelRpt_WorkbookSave (workbookHandle9, fichierLogxlsx, ExRConst_DefaultFileFormat);

	}
	else	// EXECUTION MODE
	{
		
		
		sprintf(filelog, "\\Logs.xlsx");
		strcpy(fichierLogxlsx, CurrentPath);
		strcat(fichierLogxlsx,filelog);
		ExcelRpt_WorkbookSave (workbookHandle9, fichierLogxlsx, ExRConst_DefaultFileFormat);

	}

	
	//close excel												  

	//ExcelRpt_WorkbookClose (workbookHandle4, 0);
	//ExcelRpt_WorkbookClose (workbookHandle7, 0);
	//ExcelRpt_WorkbookClose (workbookHandle1, 0);
	//ExcelRpt_WorkbookClose (workbookHandle9, 1); //1 to save changes
	//ExcelRpt_WorkbookClose (workbookHandle4, 1); */
	;
	ExcelRpt_WorkbookClose (workbookHandle9, 0);
	//ExcelRpt_ApplicationQuit (applicationLabel);

	ExcelRpt_WorkbookClose (workbookHandle4, 0);
	//ExcelRpt_ApplicationQuit (applicationHandle4);       (applicationHandle4);
	//CA_DiscardObjHandle(applicationHandle4);






	ExcelRpt_WorkbookClose (workbookHandle1, 0);
	//ExcelRpt_ApplicationQuit (applicationHandle1);
	//CA_DiscardObjHandle(applicationHandle1);


	//MODIF MaximePAGES 29/06/2020 E200
	//free(tabtoken);
	//****************

}

//***********************************************************************************************
// Modif - Carolina 04/2019
// Closing Excel
//***********************************************************************************************
void resetVariables()
{
	strcpy(label1,"");
	strcpy(label2,"");

	NBTestCheck				= 0;
	iResultCheckSTDEV 		= 0;
	iCheckAverage 		= 0; 
	iResultCheckFieldValue 	= 0;
	iCheckNoRF 				= 0;
	iCheckNbofBurst 		= 0;
	iCheckTimingInterFrames = 0;
	iCheckTimingInterBursts = 0;
	iCheckTimingFirstRF 	= 0;
	iCheckNbFramesInBurst 	= 0;
	iCheckCompareAcc		= 0;
	iCheckCompareP			= 0;
}


//Thread for RUN mode
//***********************************************************************************************
// Modif - Carolina 04/2019
// Add conditions to enable buttons and ShowCurrentScript(GiPanel, exeTestXls)
//***********************************************************************************************
static int CVICALLBACK ThreadCycleRun (void *functionData)
{
#define dDELAY_THREAD_RUN 		0.3
	
	//int iErr=0;
	CDotNetHandle exception,exp ;
	Point cell;
	int i=1;
	char exeTestXls[255];
//	labelHistory=malloc(100*sizeof(char*));
	

	char exeTestXML[MAX_PATHNAME_LEN];
	char exeNoTest[10];
	int rowNo=0;
	int exeNo=0;
	int exeStepNo=0;
	int exeStepNoRep=0;
	int exeFailedNo = 0;
	int lfCmdNo=0;
	//int fileSize;
	char exeStatus[50]="";
	int validTest=0;
	int resultTest=0;
	float EndTimeCurrentScript = 0;
	char text[500];
	char sFileNameXML[500];



	char filenameCoverageMatrix[500]="";
	
	int ongoingTest = 1;

	while (!GetGbQuitter())// && (ongoingTest == 1))
	{
		if (GetGiMode() == iMODE_RUN)
		{
			//modif carolina
			//Enable button Creation Mode and Execute Mode
			if(iAnumLoad == 1 && iDataBaseLoad == 1 && iSetLogDirLoad == 1 && countModes ==0)  //conditions to enable buttons
			{
				SetCtrlAttribute(GiPanel, PANEL_MODE_MODE_CREATION, ATTR_DIMMED, 0);
				SetCtrlAttribute(GiPanel, PANEL_MODE_MODE_EXE, ATTR_DIMMED, 0);
				SetCtrlAttribute(GiPanel, PANEL_MODE_MODE_ANALYSE, ATTR_DIMMED, 0);
				countModes = 1 ; 
			}

			DeleteTableRows (GiPanel, PANEL_MODE_SCRIPTS_DETAILS_2, 1, -1);
			SetCtrlVal(GiPanel,PANEL_MODE_ENDTIMESEQUENCE, EndTimeCurrentScript);
		//	Tend = EndTimeCurrentScript;
			//Modif MaximePAGES 1/07/2020 - Script Progress Bar E402 **
			ProgressBar_SetPercentage(GiPanel, PANEL_MODE_PROGRESSBAR, EndTimeCurrentScript, "Script Progress Bar:");
			// **
			



			if (  GetGiStartCycleRun())  //executing test sequence
			{
				GetNumTableRows(GiPanel, PANEL_MODE_TABLE_SCRIPT,&rowNo);

				char *confpath;
				char auxGsLogPath[500];

				strcpy(auxGsLogPath,  GsLogPath);
				confpath= strcat(auxGsLogPath,"\\Results.txt");

				int f = OpenFile (confpath, VAL_READ_WRITE, VAL_TRUNCATE, VAL_ASCII);

				WriteLine(f,"Results:",-1);
				WriteLine(f,"",-1);


				ShowTotalTimeAndTranslateScripts(GiPanel, rowNo);

				while(i<=rowNo && GetGiStartCycleRun())
				{

					//cleaning table current script
					DeleteTableRows (GiPanel, PANEL_MODE_SCRIPTS_DETAILS_2, 1, -1);
					SetCtrlVal(GiPanel,PANEL_MODE_ENDTIMESEQUENCE, EndTimeCurrentScript);

					//Modif MaximePAGES 1/07/2020 - Script Progress Bar E402 **
					ProgressBar_SetPercentage(GiPanel, PANEL_MODE_PROGRESSBAR, EndTimeCurrentScript, "Script Progress Bar:");
					// **
															
					cell.y = i;
					cell.x = 4;
					GetTableCellVal (GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, exeNoTest);  	//nb of tests

					cell.x = 3;
					GetTableCellVal (GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, exeTestXls);   //this column is hiden

					/*
					//Modif MaximePAGES 03/08/2020 - excel script translation parameters into values***********
					cell.x = 1;
					GetTableCellVal (GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, currentTestName);   //name of the current test


					// Modif MaximePAGES 29/07/2020 - new END Time updating with updateENDTime function
					newCurrentTestName = strtok(currentTestName,".");
					sprintf(newExeTestXls,"%s\\_TRANSLATED SCRIPTS\\%s_t.xlsx",GsLogPath,newCurrentTestName);


					updateENDTime(exeTestXls,newExeTestXls);

					strcpy(exeTestXls,newExeTestXls);
					//********************************************************************************************
					*/

					//Modif MaximePAGES 21/08/2020
					SetCtrlAttribute(GiPanel, PANEL_MODE_CLOCK_ANALYSE, ATTR_VISIBLE, 0); 
					SetCtrlAttribute (GiPanel, PANEL_MODE_CLOCK_ANALYSE, ATTR_LABEL_TEXT, "Analysing..."); 
					

					//Load current test into the table
					ShowCurrentScript(GiPanel, exeTestXls);

					exeNo=atoi(exeNoTest);
					exeStepNo=0;

					exeFailedNo = 0;
					char *exeTestCsv;

					char sFileName[500];
					SplitPath(exeTestXls,NULL,NULL,sFileName); //take the name of the test script
					WriteLine(f,sFileName,-1);

					//int a = GetGiStartCycleRun();
					exeStepNoRep = 0;
					while(exeStepNo<exeNo && GetGiStartCycleRun())	 //looking for all anum commands
					{
						//Modif MaximePAGES 16/07/2020 - RNo
						if (exeNo > 1)
						{
							exeStepNoRep=exeStepNo+1;
						}

						resultTest=0;
						validTest=0;
						//Load Test Case
						exception=0;

						//	int functionId;

						//CmtScheduleThreadPoolFunction (DEFAULT_THREAD_POOL_HANDLE, BoutonExecution, NULL, &functionId);
						//CmtWaitForThreadPoolFunctionCompletion (DEFAULT_THREAD_POOL_HANDLE, functionId, 0);
						
						WuAutoCheck_WU_Automation_Test_and_Folder_Manager_GenerateTestFile(instanceLseNetModes,exeTestXls,exeStepNoRep,&exeTestCsv,&validTest,&TabAnumCmd,&lfCmdNo,&exception);
						
						//MODIF MaximePAGES 23/06/2020 - E002
						insertElement(myListProcExcelPID,returnLastExcelPID());    //we acquire the PID Excel process
						//*************************************************************

						// Load ANum Config
						//LoadANumConfiguration(GsAnumCfgFile);

						if(validTest == 1 && GetGiStartCycleRun())
						{
							cell.y = i;
							cell.x = 2;
							sprintf(exeStatus,"Exec(%d/%d)",exeStepNo+1,exeNo);
							SetTableCellVal (GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, exeStatus);

							//load vit+press
							LoadTest(exeTestCsv,GiPanel);

							//start Anum
							ExecuteAnumCmd(dRUN,"",0,0,"");
							SetGiStartCycleAnum(lfCmdNo);

							//this button is enabled when anum is executing
							SetCtrlAttribute(GiPanel, PANEL_MODE_BUTTON_SEE_GRAPH, ATTR_DIMMED,0);
							SetCtrlAttribute (GiPanel,PANEL_MODE_BUTTON_SEE_GRAPH, ATTR_CMD_BUTTON_COLOR,0x00FF8000);
							SetCtrlAttribute(GiPanel, PANEL_MODE_MODE_CREATION, ATTR_DIMMED,1);
							SetCtrlAttribute(GiPanel, PANEL_MODE_MODE_ANALYSE, ATTR_DIMMED,1);

							OnBoutonCyclesVitessePress(GiPanel);

							//MODIF MaximePAGES 1/07/2020 - Script Progres Bar E402 *******

							ProgressBar_SetTotalTimeEstimate (GiPanel, PANEL_MODE_PROGRESSBAR, currentEndTimeGlobal);
							ProgressBar_Start(GiPanel,PANEL_MODE_PROGRESSBAR,"Script Progress Bar:");

							// *******************************************************

							while(GetGiStartCycleAnum()||GetGiStartCycleVitesse()||GetGiStartCyclePression())
								//while(GetGiStartCycleAnum()||GetGiStartVitPress())
							{

								// MODIF MaximePAGES 24/06/2020 - E403
								//dTime = Timer();
								//SyncWait(dTime, dDELAY_THREAD_RUN);
								ProcessSystemEvents();
							}
							//stop anum
							ExecuteAnumCmd(dSTOP,"",0,0,"");

							//Modif MaximePAGES 1/07/2020 - Script Progress Bar E402 **
							ProgressBar_End(GiPanel,PANEL_MODE_PROGRESSBAR,0,"Script Progress Bar:");
							// **


							//evaluate result
							SetCtrlAttribute (GiPanel,PANEL_MODE_BUT_START_RUN, ATTR_CMD_BUTTON_COLOR,0x00E0E0E0);
							DeleteGraphPlot (GiGraphPanel, PANELGRAPH_GRAPH, -1, VAL_NO_DRAW );
							SetCtrlAttribute(GiPanel, PANEL_MODE_BUTTON_SEE_GRAPH, ATTR_DIMMED,1);
							SetCtrlAttribute(GiPanel, PANEL_MODE_MODE_CREATION, ATTR_DIMMED,0);
							SetCtrlAttribute(GiPanel, PANEL_MODE_MODE_ANALYSE, ATTR_DIMMED,0); 

							exception=0;
							exp=0;
							WuAutoCheck_WU_Automation_Test_and_Folder_Manager_CheckTest(instanceLseNetModes,&resultTest,&exp);

							//MODIF MaximePAGES 23/06/2020 - E002
							insertElement(myListProcExcelPID,returnLastExcelPID());	   //we acquire the PID Excel process
							//********************************************************

							cell.y = i;
							cell.x = 2;  //write in the second column

							//modification sur le tableau
							SetTableCellVal (GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, exeStatus);




							if(resultTest==1)  //if valid test
							{

								//***********************Starting analysis************************\\

								//modif carolina
								SetCtrlAttribute(GiPanel, PANEL_MODE_CLOCK_ANALYSE, ATTR_VISIBLE, 1);

								//take path of the current dir which has been created
								char *pathLogXML;
								WuAutoCheck_WU_Automation_Test_and_Folder_Manager__getPathLog (instanceLseNetModes,&pathLogXML,&exp);
								strcpy(exeTestXML, pathLogXML);

								//Sleep(2000);
								SetSleepPolicy (VAL_SLEEP_SOME);
								//Analysis of the results
								openingExpectedResults(exeTestXls, pathLogXML,"");

								//strcpy(exeTestXML, pathLogXML);
								SplitPath(exeTestXML,NULL,NULL,sFileNameXML);

								char auxPassed[100];




								if(exeFailedNo != 0)   //when the test is not ok
								{
									SetTableCellAttribute(GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, ATTR_TEXT_BGCOLOR , VAL_RED);
									sprintf(auxPassed,"Fail(%d/%d)",exeFailedNo,exeNo);
								}
								else	//test ok
								{
									if(iResultCheckSTDEV == 0 && iResultCheckFieldValue == 0
											&& iCheckNoRF == 0
											&& iCheckTimingFirstRF == 0
											&& iCheckNbofBurst == 0
											&& iCheckNbFramesInBurst == 0
											&& iCheckTimingInterFrames == 0
											&& iCheckTimingInterBursts == 0
											&& iCheckCompareAcc	== 0
											&& iCheckCompareP	== 0
											&& iCheckAverage == 0)//tests passed
									{
										SetTableCellAttribute(GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, ATTR_TEXT_BGCOLOR , 0x0006B025);
										sprintf(auxPassed,"Passed");
										testInSeqCounter++;
										generateCoverageMatrix(pathLogXML,1);
									}
									else
									{
										SetTableCellAttribute(GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, ATTR_TEXT_BGCOLOR , VAL_RED);
										sprintf(auxPassed,"Fail(%d/%d)",exeFailedNo,exeNo);
										testInSeqCounter++;
										generateCoverageMatrix(pathLogXML,0);
									}
								}

								SetTableCellVal (GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, auxPassed);
								//Marvyn 26/07/2021
								memset(labelHistory, 0,255 * 100 );
								char testResult[500];
								char sFileName[500];

								if(iResultCheckSTDEV == 0 && iResultCheckFieldValue == 0) //tests passed
								{
									SplitPath(exeTestXls,NULL,NULL,sFileName);
									sprintf(testResult," - Exec. No.: %d / %d - Status: Passed", exeStepNo+1,exeNo);
								}
								else
								{
									SplitPath(exeTestXls,NULL,NULL,sFileName);
									sprintf(testResult," - Exec. No.: %d / %d - Status: Failed",exeStepNo+1,exeNo);
								}

								WriteLine(f,testResult, -1);
								resetVariables();
								SetCtrlAttribute(GiPanel, PANEL_MODE_CLOCK_ANALYSE, ATTR_VISIBLE, 0);
							}
							else
							{

								SetTableCellAttribute(GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, ATTR_TEXT_BGCOLOR , VAL_RED);
								char auxFail[100];

								if(exeNo != 1)sprintf(auxFail,"Fail(%d/%d)",++exeFailedNo,exeNo);
								else sprintf(auxFail,"Failed");

								SetTableCellVal (GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, auxFail);

								char testResult[500];
								char sFileName[500];

								SplitPath(exeTestXls,NULL,NULL,sFileName);
								sprintf(testResult," - Exec. No.: %d / %d - Status: Failed",exeStepNo+1,exeNo);

								WriteLine(f,testResult, -1);
								resetVariables();
								SetCtrlAttribute(GiPanel, PANEL_MODE_CLOCK_ANALYSE, ATTR_VISIBLE, 0);


							}

							//char idFileName[255] = "";
							//strcpy(idFileName, exeTestXML);
							sprintf(text, "%s - Number of tests failed : %d ", sFileNameXML, contErr);
							InsertTextBoxLine(GiPanel,PANEL_MODE_FAILEDSEQUENCE, -1, text);


							exeStepNo++;
							contErr=0;
						}
						else
						{
							exeStepNo=exeNo;
							cell.y = i;
							cell.x = 2;

							SetTableCellAttribute(GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, ATTR_TEXT_BGCOLOR , VAL_YELLOW);
							SetTableCellVal (GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, "Error");

							//take path of the current dir which has been created
							char *pathLogXMLerror;
							WuAutoCheck_WU_Automation_Test_and_Folder_Manager__getPathLog (instanceLseNetModes,&pathLogXMLerror,&exp);
							testInSeqCounter++;
							generateCoverageMatrix(pathLogXMLerror,-1);
						}

						///////////////////  MODIF MaximePAGES 23/06/2020
						killAllProcExcel(myListProcExcelPID);//kill tous les processus excel

					}

					i++;
				}
				CloseFile(f);


				//Modif MaximePAGES 15/07/2020 - Save and close CoverageMatrix Excel **********

				//GetLocalTime(&t); // Fill out the struct so that it can be used
				//sprintf(date,"%02d-%02d-%d_%02dh%02d.xlsx",t.wDay,t.wMonth,t.wYear,t.wHour,t.wMinute);

			
				strcpy(filenameCoverageMatrix,GsLogPath);
				strcat(filenameCoverageMatrix,"\\CoverageMatrix.xlsx");
				//strcat(filenameCoverageMatrix,date);

				ExcelRpt_WorkbookSave(workbookHandleCoverageMatrix, filenameCoverageMatrix, ExRConst_DefaultFileFormat);
				
				
				ExcelRpt_WorkbookClose (workbookHandleCoverageMatrix, 0);
				
				//**************************************************************************

				//Modif MaximePAGES 16/07/2020 - Seq Progress Bar **************************
				ProgressBar_End(GiPanel,PANEL_MODE_PROGRESSBARSEQ,0,"Sequence Progress Bar:");
				ProgressBar_SetPercentage(GiPanel, PANEL_MODE_PROGRESSBARSEQ, 0.0, "Sequence Progress Bar:");
				//**************************************************************************

				SetGiStartCycleRun(0);
				SetGiStartCycleAnum(0);
				
				i=1;
				ongoingTest = 0;






				//resetVariables();

				//Menu bar
				/*SetMenuBarAttribute (GetPanelMenuBar (GiPanel), CONFIG_CONFIG_ITEM_LOAD_CONF, 		ATTR_DIMMED, 	1); //Modif MaximePAGES 09/09/2020
				SetMenuBarAttribute (GetPanelMenuBar (GiPanel), CONFIG_CONFIG_ITEM_SAVE_CONF, 		ATTR_DIMMED, 	1);
				SetMenuBarAttribute (GetPanelMenuBar (GiPanel), CONFIG_CONFIG_ITEM_LOAD_ANUM_CFG, 	ATTR_DIMMED, 	1);
				SetMenuBarAttribute (GetPanelMenuBar (GiPanel), CONFIG_CONFIG_ITEM_LOAD_DATABASE, 	ATTR_DIMMED, 	1);
				SetMenuBarAttribute (GetPanelMenuBar (GiPanel), CONFIG_CONFIG_ITEM_SET_LOG_DIR, 	ATTR_DIMMED, 	1);  */

				SetCtrlAttribute (GiPanel, PANEL_MODE_BUT_START_RUN, ATTR_LABEL_TEXT, "Execute");


			}
		}
	}
	return(0);
}

// DEBUT ALGO
//***************************************************************************
//static int CVICALLBACK ThreadCycleVitesse (void *functionData)
//***************************************************************************
//  - functionData: pointeur sur des données
//
//  - Thread de gestion des cycles de vitesse
//
//  - 1 si problème détecté, 0 sinon
//***************************************************************************
// FIN ALGO
static int CVICALLBACK ThreadCycleVitesse (void *functionData)
{
#define sFORMAT_TIME_VIT(iTempsSecondes, TpsEcoule)										\
	{																						\
		iHeures = iTempsSecondes/3600;														\
		iMinutes = (iTempsSecondes/60) - (iHeures*60);										\
		iSecondes = iTempsSecondes%60;														\
		sprintf(TpsEcoule, sFORMAT_DUREE_ECOULEE, iHeures, iMinutes, iSecondes);			\
	}

#define dDELAY_THREAD_VIT 		0.2
	stConfig *ptstConfig;
	char sMsg[500];
	BOOL bDimmed1;
	BOOL bDimmed2;
	char sTpsEcoule[50];
	int iHeures;
	int iMinutes;
	int iSecondes;

	double dX1RelectCons = 0.0;
	double dY1RelectCons = 0.0;
	double dX2RelectCons = 0.0;
	double dY2RelectCons = 0.0;

	double dX1Relect = 0.0;
	double dY1Relect = 0.0;
	double dX2Relect = 0.0;
	double dY2Relect = 0.0;

	double dX1Config = 0.0;
	double dY1Config = 0.0;
	double dX2Config = 0.0;
	double dY2Config = 0.0;

	double dX1ConfigAccel = 0.0;
	double dY1ConfigAccel = 0.0;
	double dX2ConfigAccel = 0.0;
	double dY2ConfigAccel = 0.0;

	double dDureePrevue		=0.0;
	double dDureeEcouleeCycle=0.0;
	double dPente 			= 0.0;
	double dCste 			= 0.0;
	//double dPenteAcc 		= 0.0;
	//double dCsteAcc 		= 0.0;
	double dVitessePrevue 	= 0.0;
	double dDureeEcouleeRampe = 0.0;
	double dDureeEcouleeEssai = 0.0;

	char sLabel[20];
	int iErr 			= 0;
	int iNbEssais 		= 0;
	int iTimeDepart		= 0;
	int iOldStep		= 0;
	int iNumCycle 		= 0;
	int iNbCyclesPrevus = 0;
	int iIndexMeas 		= 0;
	int iHandle 		= 0;
	int iHandle2 		= 0;
	int iTimeDepartRampe = 0;
	int iIndexStepVitesse = 1;
	//int iIndexCycleVitesse = 1;
	int iTimeDepCmde;
	int iSelectionHandleVitesse = 0;
	int iErrUsb6008 	= 0;
	int iErrVaria 		= 0;

	int iTimeDepRampe=0;
	int iAccelGCmdeDelay = 1.0;

	double dProgVitDep  = 0.0;
	double dProgVitFin  = 0.0;
	double dDureeProg  	= 0.0;
	double dDureeRampe  = 0.0;
	double dLastDureeEcouleeEssai  = 0.0;
	double dDureeEcouleeCmde  	= 0.0;
	double dDureePrevueCmde		= 0.0;

	double dLastVitessePrevue 	= 0.0;
	//double dLastDureePrevue		= 0.0;
	double dDureeTotaleAcc		= 0.0;
	//double dDureePrevueVitesse	= 0.0;
	//double dLastVitesseProg		= 0.0;

	double dPlageAffichManu		= 60.0;

	double dMin  = 0.0;
	double dMax  = 0.0;

	double dTime;

	BOOL bMarcheAvant=1;
	BOOL bRampe=1;
	BOOL bAttendreConsAtteinte=0;
	BOOL bReprogMoteur=1;
	//BOOL bChgmentVitesse=0;
	short wConsigneAtteinte=1;
	float fVitesse;
	double dPression;
	double dTimeElapsed;

	//CDotNetHandle exception ;


	while (!GetGbQuitter())
	{
		if ((GetGiMode() == iMODE_AUTOMATIQUE)||(GetGiMode() == iMODE_RUN))
		{
			// Si le cycle de vitesse est lancé
			if (GetGiStartCycleVitesse())
			{
				//GbEndCycleVitesse = 0;

				SetGbEndCycleVitesse (0);

				iIndexMeas 				= 0;
				bAttendreConsAtteinte 	= 0;
				bReprogMoteur			= 0;
				SetGfVitesse (0.0);
				SetGdDurEcoulEssaiSpeed(0.0);

				ptstConfig = GetPointerToGptstConfig();
				iAccelGCmdeDelay = ptstConfig->iAccelGCmdeDelay;
				ReleasePointerToGptstConfig();

				SetGiTimeDepartEssaiSpeed(clock());

				// Effacement et tracé du graphique
				//GstGrapheMajGrapheVit(GiPanel, PANEL_MODE_TABLE_VITESSES, PANEL_MODE_GRAPHE_VITESSE, 1, GstTabVitesse.dDuree, GstTabVitesse.dVit, GstTabVitesse.iNbElmts, 1, iCOUL_CONS_VITESSE, 1);

				//modif carolina
				//GstGrapheMajGrapheVit(GiGraphPanel, PANEL_MODE_TABLE_VITESSES, PANELGRAPH_GRAPH, 1, GstTabVitesse.dDuree, GstTabVitesse.dVit, GstTabVitesse.iNbElmts, 1, iCOUL_CONS_VITESSE, 1);

				// Initialisation du numéro de cycle
				iNumCycle = 1;
				// Lecture du nombre de cycles prévus
				GetCtrlVal (GiPanel, PANEL_MODE_NBE_CYCLES_VITESSE, &iNbCyclesPrevus);

				// Affichage du numéro de cycle en cours
				SetCtrlVal (GiPanel, PANEL_MODE_NUM_CYCLE_VITESSE, iNumCycle);

				//  - Programmation de la rampe par défaut
				SetGiErrVaria(ProgRampeVariateur(0, 0, GetPointerToGptstConfig(), (double)fVIT_DEB_ARRET, (double)fVIT_FIN_ARRET, dDUREE_ARRET, GetGiUnitVitesse()));
				ReleasePointerToGptstConfig();

				//----- Modification du 15/11/2010 par C.BERENGER ----- DEBUT -----------
				dX1Relect		= 0.0;
				dY1Relect		= 0.0;
				dX2Relect		= 0.0;
				dY2Relect		= 0.0;

				iTimeDepCmde	= 0;
				//----- Modification du 15/11/2010 par C.BERENGER ----- FIN -----------

				do
				{
					// Initialisations pour chaque cycle
					if(iNumCycle == 1)
					{
						// Le moteur est à l'arrêt au premier step
						dX1Config		= 0.0;
						dY1Config		= 0.0;
						dX2Config		= GstTabVitesse.dDuree[0];
						dY2Config 		= GstTabVitesse. dVit[0];
					}
					else
					{
						// Prise en compte du dernier step du cycle précédent
						dX1Config		= 0.0;
						//CHECK_TAB_INDEX("8", GstTabVitesse.iNbElmts-1, 0, 5000)
						dY1Config		= GstTabVitesse. dVit[GstTabVitesse.iNbElmts-1];
						dX2Config		= GstTabVitesse.dDuree[0];
						dY2Config 		= GstTabVitesse. dVit[0];
					}


					dX1ConfigAccel	= 0.0;
					dY1ConfigAccel	= 0.0;
					dX2ConfigAccel	= iAccelGCmdeDelay/1000.0;
					dY2ConfigAccel	= 0.0;

					//----- Modification du 15/11/2010 par C.BERENGER ----- DEBUT -----------
					dX2Relect		= 0.0;
					dDureePrevueCmde=0.0;
					//----- Modification du 15/11/2010 par C.BERENGER ----- FIN -----------

					dDureeEcouleeCycle 	= 0.0;
					SetGiIndexStepVitesse(1);
					iIndexStepVitesse = 1;
					bReprogMoteur	= 1;
					iOldStep 		= 1;
					iTimeDepart 	= clock();
					dDureePrevue	= GstTabVitesse.dDuree[0];

					// Affichage du step en cours sur le graphique
					//iErr = DisplaySegmentActifGraph(GiPanel, PANEL_MODE_TABLE_VITESSES, PANEL_MODE_GRAPHE_VITESSE,
					//			 					iIndexStepVitesse, GstTabVitesse.iNbElmts, iNumCycle, iCOUL_CONS_VITESSE, &iSelectionHandleVitesse);

					// Affichage du step en cours dans la table
					iErr = SetTableRowAttribute (GiPanel, PANEL_MODE_TABLE_VITESSES, iIndexStepVitesse, ATTR_USE_LABEL_TEXT, 1);
					sprintf(sLabel, ">%d>", GetGiIndexStepVitesse());
					iErr = SetTableRowAttribute (GiPanel, PANEL_MODE_TABLE_VITESSES, iIndexStepVitesse, ATTR_LABEL_POINT_SIZE, 15);
					iErr = SetTableRowAttribute (GiPanel, PANEL_MODE_TABLE_VITESSES, iIndexStepVitesse, ATTR_LABEL_TEXT, sLabel);
					iErr = SetTableRowAttribute (GiPanel, PANEL_MODE_TABLE_VITESSES, iIndexStepVitesse, ATTR_LABEL_BOLD, 1);


					//-------- Modification du 15/11/2010 par C. BERENGER ----- DEBUT -------------
					//------------------------------------
					SetGiSelectionHandleVitesse(iSelectionHandleVitesse);
					//------------------------------------
					// On met toujours dans le haut du tableau la ligne en cours
					SetCtrlAttribute (GiPanel, PANEL_MODE_TABLE_VITESSES, ATTR_FIRST_VISIBLE_ROW, iIndexStepVitesse);
					//-------- Modification du 15/11/2010 par C. BERENGER ----- FIN -------------

					// On déroule le cycle de vitesse
					do
					{
						//---- Modification du 04/04/2013 par CB --- DEBUT ---------
						dTimeElapsed = (double)((clock() - GetGiTimeDepartEssaiPress())) / ((double)CLOCKS_PER_SEC);
						sprintf(sMsg, " (1)dTimeElapsed =%.02f, ------- STEP EN COURS = %d/Cycle=%d -------------",  dTimeElapsed, iIndexStepVitesse, iNumCycle);
						ModesAddMessageZM(sMsg);
						//---- Modification du 04/04/2013 par CB --- FIN ---------


						// Calcul de la durée de l'essai
						dDureeEcouleeEssai = (double)((clock() - GetGiTimeDepartEssaiSpeed())) / ((double)CLOCKS_PER_SEC);
						sFORMAT_TIME_VIT((int)dDureeEcouleeEssai, sTpsEcoule)

						iErr = SetCtrlVal (GiPanel, PANEL_MODE_AFFICH_TPS_VIT, sTpsEcoule);

						// Affichage du temps écoulé
						dDureeEcouleeCycle = (double)((clock() - iTimeDepart)) / ((double)CLOCKS_PER_SEC);

						// Incrémentation du numéro de step
						if (dDureeEcouleeCycle > dDureePrevue)
						{
							iIndexStepVitesse++;

							if (iIndexStepVitesse <= GstTabVitesse.iNbElmts)
							{
								dDureePrevue += GstTabVitesse.dDuree[iIndexStepVitesse-1];

								dX1Config	= dX2Config;
								dY1Config	= dY2Config;
								dX2Config	+= GstTabVitesse.dDuree[iIndexStepVitesse-1];
								dY2Config 	= GstTabVitesse. dVit[iIndexStepVitesse-1];

								//---- Modification du 04/04/2013 par CB --- DEBUT ---------
								dTimeElapsed = (double)((clock() - GetGiTimeDepartEssaiPress())) / ((double)CLOCKS_PER_SEC);
								sprintf(sMsg, "(2) dTimeElapsed=%.02f, dDureePrevue=%.02f, dX1Config=%0.3f, dY1Config=%0.3f, dX2Config=%0.3f, dY2Config=%0.3f",
										dTimeElapsed, dDureePrevue, dX1Config, dY1Config, dX2Config, dY2Config);
								ModesAddMessageZM(sMsg);
								//---- Modification du 04/04/2013 par CB --- FIN ---------


								// On met toujours dans le haut du tableau la ligne en cours
								SetCtrlAttribute (GiPanel, PANEL_MODE_TABLE_VITESSES, ATTR_FIRST_VISIBLE_ROW, iIndexStepVitesse);

								// Sélection du step en cours dans le tableau
								if (iOldStep != 0)
								{
									iErr = SetTableRowAttribute (GiPanel, PANEL_MODE_TABLE_VITESSES, iOldStep, ATTR_USE_LABEL_TEXT, 1);
									sprintf(sLabel, "%d", iOldStep);
									iErr = SetTableRowAttribute (GiPanel, PANEL_MODE_TABLE_VITESSES, iOldStep, ATTR_LABEL_POINT_SIZE, 11);
									iErr = SetTableRowAttribute (GiPanel, PANEL_MODE_TABLE_VITESSES, iOldStep, ATTR_LABEL_TEXT, sLabel);
									iErr = SetTableRowAttribute (GiPanel, PANEL_MODE_TABLE_VITESSES, iOldStep, ATTR_LABEL_BOLD, 0);
								}

								iErr = SetTableRowAttribute (GiPanel, PANEL_MODE_TABLE_VITESSES, iIndexStepVitesse, ATTR_USE_LABEL_TEXT, 1);
								sprintf(sLabel, ">%d>", iIndexStepVitesse);
								iErr = SetTableRowAttribute (GiPanel, PANEL_MODE_TABLE_VITESSES, iIndexStepVitesse, ATTR_LABEL_POINT_SIZE, 15);
								iErr = SetTableRowAttribute (GiPanel, PANEL_MODE_TABLE_VITESSES, iIndexStepVitesse, ATTR_LABEL_TEXT, sLabel);
								iErr = SetTableRowAttribute (GiPanel, PANEL_MODE_TABLE_VITESSES, iIndexStepVitesse, ATTR_LABEL_BOLD, 1);

								iOldStep = iIndexStepVitesse;

								bReprogMoteur = 1;
							}
						}

						//------ Programmation de la vitesse ------------
						if ((iIndexStepVitesse <= GstTabVitesse.iNbElmts) && ((GetGiUnitVitesse() != iUNIT_G) || (iAccelGCmdeDelay <= 0.0)))
						{
							//CHECK_TAB_INDEX("10", iIndexStepVitesse-1, 0, 5000)
							PilotageMoteurModeAuto (&bAttendreConsAtteinte, &wConsigneAtteinte, &bReprogMoteur,
													&dDureeRampe, iTimeDepart, iIndexStepVitesse,
													dDureePrevue, GstTabVitesse.dDuree[iIndexStepVitesse-1], dY1Config, dY2Config, &dProgVitDep,
													&dProgVitFin, &dDureeProg, &bMarcheAvant, &bRampe);
						}
						//------------------------------------------

						//----- Modification 02/2019 par Carolina ----- GiGraphPanel sur la fonction  -----------
						// Lecture et affichage de la vitesse et de la pression
						LectEtAffichSpeedAndPress(GetGiStartCycleVitesse(), GetGiStartCycleVitesse(), GiPanel, GiStatusPanel, GiGraphPanel,
												  GetPointerToGptstConfig(), &fVitesse, &dPression, GetGiUnitVitesse(), GetGiUnitPression(),
												  &iErrUsb6008, &iErrVaria);
						ReleasePointerToGptstConfig();

						SetGiErrUsb6008(iErrUsb6008);
						SetGiErrVaria(iErrVaria);

						if (iIndexStepVitesse <= GstTabVitesse.iNbElmts)
						{
							// Calcul de la vitesse à programmer
							if (((dX2Config - dX1Config) < -0.001) && ((dX2Config - dX1Config) > 0.001))
							{
								SetGdVitesseAProg(dY2Config);

								//---- Modification du 04/04/2013 par CB --- DEBUT ---------
								dTimeElapsed = (double)((clock() - GetGiTimeDepartEssaiPress())) / ((double)CLOCKS_PER_SEC);
								sprintf(sMsg, "(2.1) dTimeElapsed=%.02f, VitProg dY2Config = %.02f", dY2Config);
								ModesAddMessageZM(sMsg);
								//---- Modification du 04/04/2013 par CB --- FIN ---------
							}
							else
							{
								dPente = (dY2Config - dY1Config) / (dX2Config - dX1Config);
								dCste = dY1Config - (dPente * dX1Config);

								SetGdVitesseAProg(dPente * dDureeEcouleeCycle + dCste);

								//---- Modification du 04/04/2013 par CB --- DEBUT ---------
								dTimeElapsed = (double)((clock() - GetGiTimeDepartEssaiPress())) / ((double)CLOCKS_PER_SEC);
								sprintf(sMsg, "(2.2) dTimeElapsed=%.02f, VitProg=%.02f, dPente=%.02f, dCste=%.02f, dX1Config=%.02f, dY1Config=%.02f,dX2Config=%.02f,dY2Config=%.02f",
										dTimeElapsed, dPente * dDureeEcouleeCycle + dCste, dPente, dCste, dX1Config, dY1Config, dX2Config, dY2Config);
								ModesAddMessageZM(sMsg);
								//---- Modification du 04/04/2013 par CB --- FIN ---------
							}

							//------ Traitement spécifique au pilotage en accélération (g) ------
							if ((GetGiUnitVitesse() == iUNIT_G) && (iAccelGCmdeDelay > 0.0))
							{
								dDureeEcouleeCmde = (double)((clock() - iTimeDepCmde)) / ((double)CLOCKS_PER_SEC);

								// La commande de l'accélération se fait seulement à la cadence définie dans le fichier de configuration
								if(dDureeEcouleeCmde > dDureePrevueCmde)
								{
									if(GetGdVitesseAProg() != dY2ConfigAccel)
									{
										dDureeEcouleeCycle = (double)((clock() - iTimeDepart)) / ((double)CLOCKS_PER_SEC);

										if ((dDureeEcouleeCycle + dDureePrevueCmde) > dDureePrevue)
											dDureePrevueCmde = dDureePrevue - dDureeEcouleeCycle;
										else
											dDureePrevueCmde = iAccelGCmdeDelay / 1000.0;

										if (dDureePrevueCmde < 0.05)
											dDureePrevueCmde = 0.05;

										// Vitesse/accélération actuelle
										dX1ConfigAccel	= dDureeEcouleeCycle; // Durée actuelle
										dY1ConfigAccel	= fVitesse; // Relecture de vitesse actuelle //dY2ConfigAccel;
										// Vitesse/accélération à atteindre
										dX2ConfigAccel	= (dDureeEcouleeCycle + dDureePrevueCmde);
										dY2ConfigAccel 	= dPente * (dDureeEcouleeCycle + dDureePrevueCmde) + dCste;




										//---- Modification du 04/04/2013 par CB --- DEBUT ---------
										dTimeElapsed = (double)((clock() - GetGiTimeDepartEssaiPress())) / ((double)CLOCKS_PER_SEC);
										sprintf(sMsg, "(3)dTimeElapsed=%.02f, dDureePrevueCmde=%.02f, dX1ConfigAccel=%0.3f, dY1ConfigAccel=%0.3f, dX2ConfigAccel=%0.3f, dY2ConfigAccel=%0.3f, dPente=%.03f, dCste=%.03f",
												dTimeElapsed, dDureePrevueCmde, dX1ConfigAccel, dY1ConfigAccel, dX2ConfigAccel, dY2ConfigAccel, dPente, dCste);
										ModesAddMessageZM(sMsg);
										//---- Modification du 04/04/2013 par CB --- FIN ---------

										// Modification du 06/12/2011 par CB -- DEBUT -------------
										if(dPente > 0.0)
										{
											if(dY2ConfigAccel > dY2Config)
												dY2ConfigAccel = dY2Config;
										}
										else
										{
											if(dY2ConfigAccel < dY2Config)
												dY2ConfigAccel = dY2Config;
										}
										// Modification du 06/12/2011 par CB -- FIN -------------

										// Modification du 06/12/2011 par CB -- DEBUT -------------
										if (fabs(dY2ConfigAccel - dY1ConfigAccel) < 0.05)
											if(dPente < 0)
												dY2ConfigAccel = dY1ConfigAccel - 0.05;
											else
												dY2ConfigAccel = dY1ConfigAccel + 0.05;
										// Modification du 06/12/2011 par CB -- FIN -------------


										// Programmation de la vitesse
										bReprogMoteur = 1;
									}
								}
							}

							if ((iIndexStepVitesse <= GstTabVitesse.iNbElmts) && (GetGiUnitVitesse() == iUNIT_G) && (iAccelGCmdeDelay > 0.0))
							{
								if(bReprogMoteur)
									iTimeDepCmde = clock();

								PilotageMoteurModeAuto (&bAttendreConsAtteinte, &wConsigneAtteinte, &bReprogMoteur,
														&dDureeRampe, iTimeDepart, iIndexStepVitesse,
														// Dure prevue
														dX2ConfigAccel, dDureePrevueCmde, dY1ConfigAccel, dY2ConfigAccel, &dProgVitDep,
														&dProgVitFin, &dDureeProg, &bMarcheAvant, &bRampe);
							}

							//---------------------------------------------------------------------

							dX1Relect		= dX2Relect;
							dY1Relect		= dY2Relect;
							dX2Relect		= dDureeEcouleeCycle;
							dY2Relect		= (double)fVitesse;

							// Affichage de la vitesse sur le graphique
							iHandle = PlotLine (GiPanel, PANEL_MODE_GRAPHE_VITESSE, dX1Relect, dY1Relect, dX2Relect, dY2Relect, iCOUL_VITESSE);
							SetPlotAttribute (GiPanel, PANEL_MODE_GRAPHE_VITESSE, iHandle, ATTR_PLOT_THICKNESS, 2);

							//modif carolina
							if(flag_seegraph == 1)
							{
								iHandle2 = PlotLine (GiGraphPanel, PANELGRAPH_GRAPH, dX1Relect, dY1Relect, dX2Relect, dY2Relect, iCOUL_VITESSE);
								SetPlotAttribute (GiGraphPanel, PANELGRAPH_GRAPH, iHandle2, ATTR_PLOT_THICKNESS, 2);
								if(flag_seegraph != 1)
								{
									iHandle2 = 0;
								}
							}
							iIndexMeas++;

							//---- Modification du 04/04/2013 par CB --- DEBUT ---------
							dTimeElapsed = (double)((clock() - GetGiTimeDepartEssaiPress())) / ((double)CLOCKS_PER_SEC);
							sprintf(sMsg, "(4) dTimeElapsed=%.02f, dX1Relect=%0.3f, dY1Relect=%0.3f, dX2Relect=%0.3f, dY2Relect=%0.3f",
									dTimeElapsed, dX1Relect, dY1Relect, dX2Relect, dY2Relect);
							ModesAddMessageZM(sMsg);
							//---- Modification du 04/04/2013 par CB --- FIN ---------
						}

						// Mise à jour des variables globales
						//-----------------------------------
						SetGfVitesse (fVitesse);
						SetGdPression(dPression);
						SetGdDurEcoulEssaiSpeed(dDureeEcouleeEssai);

						if(iIndexStepVitesse <= GstTabVitesse.iNbElmts)
							SetGiIndexStepVitesse(iIndexStepVitesse);

						if(iNumCycle <= iNbCyclesPrevus)
							SetGiIndexCycleVitesse(iNumCycle);
						//-----------------------------------

						ProcessSystemEvents();
						dTime = Timer();
						SyncWait(dTime, dDELAY_THREAD_VIT);
					}
					while((GetGiStartCycleVitesse()) && (iIndexStepVitesse <= GstTabVitesse.iNbElmts));

					if (iOldStep != 0)
					{
						iErr = SetTableRowAttribute (GiPanel, PANEL_MODE_TABLE_VITESSES, iOldStep, ATTR_USE_LABEL_TEXT, 1);
						sprintf(sLabel, "%d", iOldStep);
						iErr = SetTableRowAttribute (GiPanel, PANEL_MODE_TABLE_VITESSES, iOldStep, ATTR_LABEL_POINT_SIZE, 11);
						iErr = SetTableRowAttribute (GiPanel, PANEL_MODE_TABLE_VITESSES, iOldStep, ATTR_LABEL_TEXT, sLabel);
						iErr = SetTableRowAttribute (GiPanel, PANEL_MODE_TABLE_VITESSES, iOldStep, ATTR_LABEL_BOLD, 0);
					}

					// Raz du temps des vitesses
					SetGbRazTimeVit(1);

					if (GetGiStartCycleVitesse() == 1)
						// Incrémentation du numéro de cycle en cours
						iNumCycle++;

					if (iNumCycle <= iNbCyclesPrevus)
					{
						// Affichage du numéro de cycle en cours
						SetCtrlVal (GiPanel, PANEL_MODE_NUM_CYCLE_VITESSE, iNumCycle);

						/*if (GetGiStartCycleVitesse() == 1)
							// Effacement et tracé du graphique
							//GstGrapheMajGraphePres(GiPanel, PANEL_MODE_TABLE_VITESSES, PANEL_MODE_GRAPHE_VITESSE, 1, GstTabVitesse.dDuree, GstTabVitesse.dVit, GstTabVitesse.iNbElmts, iNumCycle, iCOUL_CONS_VITESSE, 1);
						//GstGrapheMajGraphePres(GiGraphPanel, PANEL_MODE_TABLE_VITESSES, PANELGRAPH_GRAPH, 1, GstTabVitesse.dDuree, GstTabVitesse.dVit, GstTabVitesse.iNbElmts, iNumCycle, iCOUL_CONS_VITESSE, 1);
						*/
					}

					ProcessSystemEvents();
					dTime = Timer();
					SyncWait(dTime, dDELAY_THREAD_VIT);
				}
				while ((iNumCycle <= iNbCyclesPrevus) && (GetGiStartCycleVitesse() == 1));

				SetGiTimeDepartEssaiSpeed(0);

				//  - Arrêt du moteur
				SetGiErrVaria(ProgRampeVariateur(0, 0, GetPointerToGptstConfig(), (double)fVIT_DEB_ARRET, (float)fVIT_FIN_ARRET, dDUREE_ARRET, GetGiUnitVitesse()));
				ReleasePointerToGptstConfig();
				SetGiErrVaria(VariateurArret());

				GetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VITESSE, 	ATTR_DIMMED, &bDimmed1);
				GetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VIT_PRESS, ATTR_DIMMED, &bDimmed2);
				SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VITESSE, 	ATTR_DIMMED, 1);
				SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VIT_PRESS, ATTR_DIMMED, 1);

				// Attente d'arrêt du moteur
				iNbEssais = 0;
				SetGdVitesseAProg(0.0);
				do
				{
					iNbEssais++;
					// Lecture et affichage de la vitesse et de la pression
					LectEtAffichSpeedAndPress(1, 1, GiPanel, GiStatusPanel, GiGraphPanel,
											  GetPointerToGptstConfig(), &fVitesse, &dPression, GetGiUnitVitesse(), GetGiUnitPression(),
											  &iErrUsb6008, &iErrVaria);
					ReleasePointerToGptstConfig();

					SetGiErrUsb6008(iErrUsb6008);
					SetGiErrVaria(iErrVaria);
					SetGfVitesse (fVitesse);
					SetGdPression(dPression);

					ProcessSystemEvents();
					dTime = Timer();
					SyncWait(dTime, dDELAY_THREAD_VIT);
				}
				while (((fabs(fVitesse) > dVIT_ABS_START_TEST) && (iNbEssais < 30)) && (!GetGbQuitter()));

				SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VITESSE, ATTR_DIMMED,   bDimmed1);
				SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VIT_PRESS, ATTR_DIMMED, bDimmed2);

				dTime = Timer();
				SyncWait(dTime, 0.5);

				// Arrêt de la ventilation moteur
				SetGiErrUsb6008(Usb6008CommandeVentilationMoteur (iUSB6008_CMDE_VENTILATION_MOTEUR_INACTIFS));

				SetGiStartCycleVitesse(0);
				// Fin du cycle
				iErr = GstIhmCycles(GiPanel,GetGiStartVitPress(),  GetGiStartCycleVitesse(), GetGiStartCyclePression());

				SetGbEndCycleVitesse (1);

			}
		}
		else
		{
			//--------- MODE MANUEL --------
			dX1Config 		= 0.0;
			dY1Config 		= 0.0;
			dX2Config 		= 0.0;
			dY2Config 		= 0.0;

			//------- Modif du 15/11/2010 par C.BERENGER ------ DEBUT ------------
			dX1ConfigAccel	= 0.0;
			dY1ConfigAccel	= 0.0;
			ptstConfig 		= GetPointerToGptstConfig();
			iAccelGCmdeDelay = ptstConfig->iAccelGCmdeDelay;
			dPlageAffichManu =  ptstConfig->dPlageAffichManu;
			ReleasePointerToGptstConfig();

			dX2ConfigAccel	= iAccelGCmdeDelay/1000.0;
			dY2ConfigAccel	= 0.0;
			iTimeDepCmde	= 0;
			//------- Modif du 15/11/2010 par C.BERENGER ------ FIN ------------

			dX1Relect		= 0.0;
			dY1Relect		= 0.0;
			dX2Relect		= 0.0;
			dY2Relect		= 0.0;

			dX1RelectCons	= 0.0;
			dY1RelectCons	= 0.0;
			dX2RelectCons	= 0.0;
			dY2RelectCons	= 0.0;

			dPente 			= 0.0;
			dCste			= 0.0;

			dMin			= 0.0;
			dMax			= 0.0;

			dLastDureeEcouleeEssai = 0.0;

			SetGdVitesseAProg(-1.0);
			bAttendreConsAtteinte = 0;

			dLastVitessePrevue 	= 0.0;
			dDureeEcouleeRampe 	= 0.0;
			dDureeTotaleAcc		= 0.0;
			iTimeDepRampe		= 0;

			SetGdVitesseAProg(0.0);
			SetGiIndexStepVitesse(0);
			SetGiIndexCycleVitesse(0);

			if (GetGiStartCycleVitesse())
			{
				SetGbEndCycleVitesse (0);

				//------- Modif du 15/11/2010 par C.BERENGER ------ DEBUT ------------
				SetAxisScalingMode (GiPanel, PANEL_MODE_GRAPHE_VITESSE, VAL_BOTTOM_XAXIS, VAL_AUTOSCALE, 0.0, 100.0);
				//------- Modif du 15/11/2010 par C.BERENGER ------ FIN ------------

				// Reset du graphique
				DeleteGraphPlot (GiPanel, PANEL_MODE_GRAPHE_VITESSE, -1, VAL_IMMEDIATE_DRAW);

				if(flag_seegraph == 1)
				{
					//modif carolina
					SetAxisScalingMode (GiGraphPanel, PANELGRAPH_GRAPH, VAL_BOTTOM_XAXIS, VAL_AUTOSCALE, 0.0, 100.0);
					DeleteGraphPlot (GiGraphPanel, PANELGRAPH_GRAPH, -1, VAL_IMMEDIATE_DRAW);
				}

				// Temps de départ
				SetGiTimeDepartEssaiSpeed(clock());
				iTimeDepartRampe = clock();
				iTimeDepart 	 = clock();

				//  - Programmation de la rampe par défaut
				SetGiErrVaria(ProgRampeVariateur(0, 0, GetPointerToGptstConfig(), (double)fVIT_DEB_ARRET, (double)fVIT_FIN_ARRET, dDUREE_ARRET, GetGiUnitVitesse()));
				ReleasePointerToGptstConfig();

				// Si le cycle de vitesse est lancé
				do
				{
					// Lecture de la valeur de vitesse à programmer
					GetCtrlVal (GiPanel, PANEL_MODE_SAISIE_VITESSE_MANU, &dVitessePrevue);

					// Si la vitesse a été modifiée
					if ((dY2Config != dVitessePrevue) || (bAttendreConsAtteinte))
					{
						// S'il faut attendre la consigne
						if ((bAttendreConsAtteinte) && (dY2Config == dVitessePrevue))
						{
							//  - Détermination consigne vitesse atteinte
							SetGiErrVaria(VariateurConsigneVitesseAtteinte (&wConsigneAtteinte));

							// Si la consigne est atteinte
							if (wConsigneAtteinte)
							{
								dDureeRampe = dDureePrevue - ((double)((clock() - iTimeDepartRampe)) / ((double)CLOCKS_PER_SEC));
								iErr = CalcParamVitesse(0.0, dY2Config, dDureeRampe,
														&dProgVitDep, &dProgVitFin, &dDureeProg,
														&bMarcheAvant, &bRampe, &bAttendreConsAtteinte);

								if (bRampe)
									//  - Programmation d'une rampe de vitesse
									SetGiErrVaria(ProgRampeVariateur(0, 1, GetPointerToGptstConfig(), dProgVitDep, dProgVitFin, dDureeProg, GetGiUnitVitesse()));
								else
									//  - Programmation d'une rampe de vitesse à la vitesse max
									SetGiErrVaria(ProgRampeVariateur(0, 0, GetPointerToGptstConfig(), (double)fVIT_DEB_ARRET, (double)fVIT_FIN_ARRET, dDUREE_ARRET, GetGiUnitVitesse()));
								ReleasePointerToGptstConfig();

								if (bMarcheAvant)
									//  - Emission consigne de vitesse et mise en route du variateur
									SetGiErrVaria(CmdeVitesseVariateur(GetPointerToGptstConfig(), dProgVitFin, iVARIATEUR_MARCHE_AVANT, GetGiUnitVitesse()));
								else
									//  - Emission consigne de vitesse et mise en route du variateur
									SetGiErrVaria(CmdeVitesseVariateur(GetPointerToGptstConfig(), dProgVitFin, iVARIATEUR_MARCHE_ARRIERE, GetGiUnitVitesse()));
								ReleasePointerToGptstConfig();
							}
						}
						else
						{
							GetCtrlVal (GiPanel, PANEL_MODE_SAISIE_DUREE_VIT_MANU, &dDureePrevue);

							dX1Config = 0.0;
							dX2Config = dDureePrevue;
							dY1Config = dY2RelectCons; //dY2Config;
							dY2Config = dVitessePrevue;

							// Calcul de la vitesse à programmer
							if (((dX2Config - dX1Config) > -0.001) && ((dX2Config - dX1Config) < 0.001))
							{
								dPente 	= 0.0;
								dCste 	= 0.0;
								SetGdVitesseAProg(dY2Config);
							}
							else
							{
								dPente = (dY2Config - dY1Config) / (dX2Config - dX1Config);
								dCste = dY1Config - (dPente * dX1Config);
							}

							// Mise à zéro de la durée écoulée
							dDureeEcouleeCycle = 0.0;
							// Temps de départ
							iTimeDepartRampe = clock();

							// Calcul des paramètres de programmation du moteur
							iErr = CalcParamVitesse(dY1Config, dY2Config, dDureePrevue,
													&dProgVitDep, &dProgVitFin, &dDureeProg,
													&bMarcheAvant, &bRampe, &bAttendreConsAtteinte);

#ifdef bSIMULATION
							if(bRampe)
								SetCtrlAttribute (GetSimuHandle(), PANEL_SIMU_RAMPE_ACCELERATION, ATTR_DIMMED, 0);
							else
								SetCtrlAttribute (GetSimuHandle(), PANEL_SIMU_RAMPE_ACCELERATION, ATTR_DIMMED, 1);
#endif

							if (bRampe)
								//  - Programmation d'une rampe de vitesse
								SetGiErrVaria(ProgRampeVariateur(0, 1, GetPointerToGptstConfig(), dProgVitDep, dProgVitFin, dDureeProg, GetGiUnitVitesse()));
							else
								//  - Programmation d'une rampe de vitesse à la vitesse max
								SetGiErrVaria(ProgRampeVariateur(0, 0, GetPointerToGptstConfig(), (double)fVIT_DEB_ARRET, (double)fVIT_FIN_ARRET, dDUREE_ARRET, GetGiUnitVitesse()));
							ReleasePointerToGptstConfig();

							if (bMarcheAvant)
								//  - Emission consigne de vitesse et mise en route du variateur
								SetGiErrVaria(CmdeVitesseVariateur(GetPointerToGptstConfig(), dProgVitFin, iVARIATEUR_MARCHE_AVANT, GetGiUnitVitesse()));
							else
								//  - Emission consigne de vitesse et mise en route du variateur
								SetGiErrVaria(CmdeVitesseVariateur(GetPointerToGptstConfig(), dProgVitFin, iVARIATEUR_MARCHE_ARRIERE, GetGiUnitVitesse()));
							ReleasePointerToGptstConfig();
						}
					}

					dDureeEcouleeRampe = (double)((clock() - iTimeDepartRampe)) / ((double)CLOCKS_PER_SEC);

					// Rampe
					if (dDureeEcouleeRampe <= dDureePrevue)
					{
						// Calcul de la pression à programmer
						SetGdVitesseAProg(dPente * dDureeEcouleeRampe + dCste);
					}
					else
						// Palier
						SetGdVitesseAProg(dVitessePrevue);


					// Calcul de la durée de l'essai
					dDureeEcouleeEssai = (double)((clock() - GetGiTimeDepartEssaiSpeed())) / ((double)CLOCKS_PER_SEC);
					//Affichage de la durée de l'essai à l'écran
					sFORMAT_TIME_VIT((int)dDureeEcouleeEssai, sTpsEcoule)

					SetCtrlVal (GiPanel, PANEL_MODE_AFFICH_TPS_VIT, sTpsEcoule);

					// Lecture et affichage de la vitesse et de la pression
					LectEtAffichSpeedAndPress(GetGiStartCycleVitesse(), GetGiStartCycleVitesse(), GiPanel, GiStatusPanel, GiGraphPanel,
											  GetPointerToGptstConfig(), &fVitesse, &dPression, GetGiUnitVitesse(), GetGiUnitPression(),
											  &iErrUsb6008, &iErrVaria);
					ReleasePointerToGptstConfig();

					SetGiErrUsb6008(iErrUsb6008);
					SetGiErrVaria(iErrVaria);


					// Calcul de la consigne de vitesse sur le graphique
					dX1RelectCons	= dX2RelectCons;
					dY1RelectCons	= dY2RelectCons;
					dX2RelectCons	= dDureeEcouleeEssai;
					dY2RelectCons	= GetGdVitesseAProg();

					// Affichage de la consigne de vitesse sur le graphique
					iHandle = PlotLine (GiPanel, PANEL_MODE_GRAPHE_VITESSE, dX1RelectCons, dY1RelectCons, dX2RelectCons, dY2RelectCons, iCOUL_CONS_VITESSE);
					SetPlotAttribute (GiPanel, PANEL_MODE_GRAPHE_VITESSE, iHandle, ATTR_PLOT_THICKNESS, 1);

					//modif carolina
					if(flag_seegraph == 1)
					{
						iHandle2 = PlotLine (GiGraphPanel, PANELGRAPH_GRAPH, dX1RelectCons, dY1RelectCons, dX2RelectCons, dY2RelectCons, iCOUL_CONS_VITESSE);
						SetPlotAttribute (GiGraphPanel, PANELGRAPH_GRAPH, iHandle2, ATTR_PLOT_THICKNESS, 1);
						if(flag_seegraph != 1)
						{
							iHandle2 = 0;
						}
					}

					// Mise à jour des variables globales
					//-----------------------------------
					SetGfVitesse (fVitesse);
					SetGdPression(dPression);
					SetGdDurEcoulEssaiSpeed(dDureeEcouleeEssai);
					//-----------------------------------


					// Calcul de la relecture de vitesse sur le graphique
					dX1Relect	= dX2Relect;
					dY1Relect	= dY2Relect;
					dX2Relect	= dDureeEcouleeEssai;
					dY2Relect	= GetGfVitesse ();

					//------- Modif du 15/11/2010 par C.BERENGER ------ DEBUT ------------
					if(dY2RelectCons < dMin) dMin = dY2RelectCons;
					if(dY2RelectCons > dMax) dMax = dY2RelectCons;
					if(dY2Relect < dMin) dMin = dY2Relect;
					if(dY2Relect > dMax) dMax = dY2Relect;

					if(dDureeEcouleeEssai > dPlageAffichManu)
						if(dDureeEcouleeEssai - dLastDureeEcouleeEssai > dPlageAffichManu)
						{
							// Reset du graphique
							DeleteGraphPlot (GiPanel, PANEL_MODE_GRAPHE_VITESSE, -1, VAL_IMMEDIATE_DRAW);
							SetAxisScalingMode (GiPanel, PANEL_MODE_GRAPHE_VITESSE, VAL_BOTTOM_XAXIS, VAL_MANUAL, dDureeEcouleeEssai, dDureeEcouleeEssai + dPlageAffichManu);
							// Points pour garder l'échelle précédente car autoscale vertical
							PlotPoint (GiPanel, PANEL_MODE_GRAPHE_VITESSE, dDureeEcouleeEssai, dMax, VAL_NO_POINT, VAL_TRANSPARENT);
							PlotPoint (GiPanel, PANEL_MODE_GRAPHE_VITESSE, dDureeEcouleeEssai, dMin, VAL_NO_POINT, VAL_TRANSPARENT);

							if(flag_seegraph == 1)
							{
								//modif carolina
								DeleteGraphPlot (GiGraphPanel, PANELGRAPH_GRAPH, -1, VAL_IMMEDIATE_DRAW);
								SetAxisScalingMode (GiGraphPanel, PANELGRAPH_GRAPH, VAL_BOTTOM_XAXIS, VAL_MANUAL, dDureeEcouleeEssai, dDureeEcouleeEssai + dPlageAffichManu);
								PlotPoint (GiGraphPanel, PANELGRAPH_GRAPH, dDureeEcouleeEssai, dMax, VAL_NO_POINT, VAL_TRANSPARENT);
								PlotPoint (GiGraphPanel, PANELGRAPH_GRAPH, dDureeEcouleeEssai, dMin, VAL_NO_POINT, VAL_TRANSPARENT);
							}

							dLastDureeEcouleeEssai =  dDureeEcouleeEssai;
						}
					//------- Modif du 15/11/2010 par C.BERENGER ------ FIN ------------

					// Affichage de la relecture de vitesse sur le graphique
					iHandle = PlotLine (GiPanel, PANEL_MODE_GRAPHE_VITESSE, dX1Relect, dY1Relect, dX2Relect, dY2Relect, iCOUL_VITESSE);
					SetPlotAttribute (GiPanel, PANEL_MODE_GRAPHE_VITESSE, iHandle, ATTR_PLOT_THICKNESS, 2);

					//modif carolina
					if(flag_seegraph == 1)
					{
						iHandle2 = PlotLine (GiGraphPanel, PANELGRAPH_GRAPH, dX1RelectCons, dY1RelectCons, dX2RelectCons, dY2RelectCons, iCOUL_CONS_VITESSE);
						SetPlotAttribute (GiGraphPanel, PANELGRAPH_GRAPH, iHandle2, ATTR_PLOT_THICKNESS, 2);
						if(flag_seegraph != 1)
						{
							iHandle2 = 0;
						}
					}

					ProcessSystemEvents();
					dTime = Timer();
					SyncWait(dTime, dDELAY_THREAD_VIT);

					if(GetGiStartCycleVitesse() == 0)
					{
						SetGiTimeDepartEssaiSpeed(0);
						//  - Mise à zéro de la vitesse moteur
						SetGiErrVaria(ProgRampeVariateur(0, 0, GetPointerToGptstConfig(), (double)fVIT_DEB_ARRET, (double)fVIT_FIN_ARRET, dDUREE_ARRET, GetGiUnitVitesse()));
						ReleasePointerToGptstConfig();
						SetGiErrVaria(VariateurArret());

						GetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VITESSE, 	ATTR_DIMMED, &bDimmed1);
						GetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VIT_PRESS, ATTR_DIMMED, &bDimmed2);
						SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VITESSE, 	ATTR_DIMMED, 1);
						SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VIT_PRESS, ATTR_DIMMED, 1);

						// Attente d'arrêt du moteur
						iNbEssais = 0;
						SetGdVitesseAProg(0.0);

						do
						{
							//GiErrVaria = VariateurConsigneVitesseAtteinte (&wConsigneAtteinte);
							iNbEssais++;
							// Lecture et affichage de la vitesse et de la pression
							LectEtAffichSpeedAndPress(1, 1, GiPanel, GiStatusPanel,GiGraphPanel,
													  GetPointerToGptstConfig(), &fVitesse, &dPression, GetGiUnitVitesse(), GetGiUnitPression(),
													  &iErrUsb6008, &iErrVaria);
							ReleasePointerToGptstConfig();

							SetGiErrUsb6008(iErrUsb6008);
							SetGiErrVaria(iErrVaria);
							SetGfVitesse (fVitesse);
							SetGdPression(dPression);

							dTime = Timer();
							SyncWait(dTime, dDELAY_THREAD_VIT);
							ProcessSystemEvents();
						}
						while (((fabs(fVitesse) > dVIT_ABS_START_TEST) && (iNbEssais < 30)) && (!GetGbQuitter()));

						// Arrêt de la ventilation
						SetGiErrUsb6008(Usb6008CommandeVentilationMoteur (iUSB6008_CMDE_VENTILATION_MOTEUR_INACTIFS));

						SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VITESSE, ATTR_DIMMED,   bDimmed1);
						SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VIT_PRESS, ATTR_DIMMED, bDimmed2);
					}
				}
				while(GetGiStartCycleVitesse());

				SetGbEndCycleVitesse (1);
			}
		}
		dTime = Timer();
		SyncWait(dTime, dDELAY_THREAD_VIT);
		ProcessSystemEvents();
	}

	return(0);
}

// DEBUT ALGO
//***************************************************************************
//static int CVICALLBACK ThreadCyclePression (void *functionData)
//***************************************************************************
//  - functionData: pointeur sur des données
//
//  - Thread de gestion des cycles de pression
//
//  - 0 si OK, sinon 1
//***************************************************************************
// FIN ALGO
static int CVICALLBACK ThreadCyclePression (void *functionData)
{
#define sFORMAT_TIME_PRESS(iTempsSecondes, TpsEcoule)									\
	{																						\
		iHeures = iTempsSecondes/3600;														\
		iMinutes = (iTempsSecondes/60) - (iHeures*60);										\
		iSecondes = iTempsSecondes%60;														\
		sprintf(TpsEcoule, sFORMAT_DUREE_ECOULEE, iHeures, iMinutes, iSecondes);			\
	}

#define dDELAY_THREAD_PRESS 0.2
	BOOL bDimmed1;
	BOOL bDimmed2;
	char sTpsEcoule[50];
	int iHeures;
	int iMinutes;
	int iSecondes;
	float fVitesse=0.0;
	double dPression=0.0;
	double dX1RelectCons;
	double dY1RelectCons;
	double dX2RelectCons;
	double dY2RelectCons;

	double dX1Relect;
	double dY1Relect;
	double dX2Relect;
	double dY2Relect;

	double dX1Config;
	double dY1Config;
	double dX2Config;
	double dY2Config;
	double dDureePrevue=0.0;
	double dDureeEcouleeCycle=0.0;
	double dDureeEcouleeRampe=0.0;
	double dDureeEcouleeEssai=0.0;
	double dDureeEcouleeCmde=0.0;

	double dPente;
	double dCste;
	double dPressionPrevue;
	double dDelayCmdPress;
	double dLastDureeEcouleeEssai;
	double dMin;
	double dMax;

	double dTime;

	double dPlageAffichManu = 60.0;
	char sLabel[20];
	int iErr;
	int iTimeDepCmde;
	int iTimeDepart;
	int iTimeDepartRampe;
	int iOldStep=0;
	int iNumCycle=1;
	int iNbCyclesPrevus=1;
	int iIndexStepPression=1;
	int iIndexMeas=1;
	int iHandle=0;
	int iHandle2=0;
	int iNbEssais=1;
	int iSelectionHandlePression=0;
	int iErrUsb6008=0;
	int iErrVaria=0;
	stConfig *ptstConfig;

	ptstConfig = GetPointerToGptstConfig();
	dDelayCmdPress = ((double)ptstConfig->iPressCommandDelay) / 1000;
	dPlageAffichManu = ptstConfig->dPlageAffichManu;
	ReleasePointerToGptstConfig();

	iTimeDepCmde = 0;

	while (!GetGbQuitter())
	{
		//--------- MODE AUTOMATIQUE --------
		if ((GetGiMode() == iMODE_AUTOMATIQUE)||(GetGiMode() == iMODE_RUN))
		{
			// Si le cycle de pression est lancé
			if (GetGiStartCyclePression())
			{
				SetGbEndCyclePression(0);

				// Effacement et tracé du graphique
				//GstGrapheMajGraphePres(GiPanel, PANEL_MODE_TABLE_PRESSION, PANEL_MODE_GRAPHE_VITESSE, 1, GstTabPression.dDuree, GstTabPression.dPress, GstTabPression.iNbElmts, 1, iCOUL_CONS_PRESSION, 1);
				//modif carolina
				//GstGrapheMajGraphePres(GiGraphPanel, PANEL_MODE_TABLE_PRESSION, PANELGRAPH_GRAPH, 1, GstTabPression.dDuree, GstTabPression.dPress, GstTabPression.iNbElmts, 1, iCOUL_CONS_PRESSION, 1);

				iIndexMeas = 0;
				SetGiTimeDepartEssaiPress(clock());

				// Initialisation du numéro de cycle
				iNumCycle = 1;

				// Lecture du nombre de cycles prévus
				GetCtrlVal (GiPanel, PANEL_MODE_NBE_CYCLES_PRESSION, &iNbCyclesPrevus);

				// Affichage du numéro de cycle en cours
				SetCtrlVal (GiPanel, PANEL_MODE_NUM_CYCLE_PRESSION, iNumCycle);

				//----- Modification du 15/11/2010 par C.BERENGER ----- DEBUT -----------
				dX1Relect		= 0.0;
				dY1Relect		= 0.0;
				dX2Relect		= 0.0;
				dY2Relect		= 0.0;
				//----- Modification du 15/11/2010 par C.BERENGER ----- FIN -----------


				do
				{
					// Initialisations pour chaque cycle
					if(iNumCycle == 1)
					{
						dX1Config		= 0.0;
						dY1Config		= 0.0;
						dX2Config		= GstTabPression.dDuree[0];
						dY2Config 		= GstTabPression.dPress[0];
					}
					else
					{
						dX1Config		= 0.0;
						dY1Config		= GstTabPression.dPress[GstTabPression.iNbElmts-1];
						dX2Config		= GstTabPression.dDuree[0];
						dY2Config 		= GstTabPression.dPress[0];
					}

					//----- Modification du 15/11/2010 par C.BERENGER ----- DEBUT -----------
					dX2Relect		= 0.0;
					//----- Modification du 15/11/2010 par C.BERENGER ----- FIN -----------

					dDureeEcouleeCycle= 0.0;
					SetGiIndexStepPression(1);
					iIndexStepPression	= 1;
					iOldStep		= 1;
					iTimeDepart 	= clock();
					dDureePrevue 	= GstTabPression.dDuree[0];

					// Affichage du step en cours sur le graphique
					//iErr = DisplaySegmentActifGraph(GiPanel, PANEL_MODE_TABLE_PRESSION, PANEL_MODE_GRAPHE_PRESSION,
					//						 		iIndexStepPression, GstTabPression.iNbElmts, iNumCycle, iCOUL_CONS_PRESSION, &iSelectionHandlePression);

					iErr = SetTableRowAttribute (GiPanel, PANEL_MODE_TABLE_PRESSION, iIndexStepPression, ATTR_USE_LABEL_TEXT, 1);
					sprintf(sLabel, ">%d>", iIndexStepPression);
					iErr = SetTableRowAttribute (GiPanel, PANEL_MODE_TABLE_PRESSION, iIndexStepPression, ATTR_LABEL_POINT_SIZE, 15);
					iErr = SetTableRowAttribute (GiPanel, PANEL_MODE_TABLE_PRESSION, iIndexStepPression, ATTR_LABEL_TEXT, sLabel);
					iErr = SetTableRowAttribute (GiPanel, PANEL_MODE_TABLE_PRESSION, iIndexStepPression, ATTR_LABEL_BOLD, 1);
					//-------- Modification du 15/11/2010 par C. BERENGER ----- DEBUT -------------
					// On met toujours dans le haut du tableau la ligne en cours
					SetCtrlAttribute (GiPanel, PANEL_MODE_TABLE_PRESSION, ATTR_FIRST_VISIBLE_ROW, iIndexStepPression);

					//-------- Modification du 15/11/2010 par C. BERENGER ----- FIN -------------

					// On déroule le cycle de pression
					do
					{
						// Calcul de la durée de l'essai
						dDureeEcouleeEssai = (double)((clock() - GetGiTimeDepartEssaiPress())) / ((double)CLOCKS_PER_SEC);
						//Affichage de la durée de l'essai à l'écran
						sFORMAT_TIME_PRESS((int)dDureeEcouleeEssai, sTpsEcoule)

						SetCtrlVal (GiPanel, PANEL_MODE_AFFICH_TPS_PRESS, sTpsEcoule);

						// Affichage du temps écoulé
						dDureeEcouleeCycle = (double)((clock() - iTimeDepart)) / ((double)CLOCKS_PER_SEC);

						// Incrémentation du numéro de step
						if (dDureeEcouleeCycle > dDureePrevue)
						{
							iIndexStepPression++;

							if (iIndexStepPression <= GstTabPression.iNbElmts)
							{
								dDureePrevue += GstTabPression.dDuree[iIndexStepPression-1];

								dX1Config	= dX2Config;
								dY1Config	= dY2Config;
								dX2Config	+= GstTabPression.dDuree[iIndexStepPression-1];
								dY2Config 	= GstTabPression.dPress[iIndexStepPression-1];

								// Affichage du step en cours sur le graphique
								//iErr = DisplaySegmentActifGraph(GiPanel, PANEL_MODE_TABLE_PRESSION, PANEL_MODE_GRAPHE_PRESSION,
								//						 		iIndexStepPression, GstTabPression.iNbElmts, iNumCycle,iCOUL_CONS_PRESSION, &iSelectionHandlePression);

								// On met toujours dans le haut du tableau la ligne en cours
								SetCtrlAttribute (GiPanel, PANEL_MODE_TABLE_PRESSION, ATTR_FIRST_VISIBLE_ROW, iIndexStepPression);

								// Sélection du step en cours dans le tableau
								if (iOldStep != 0)
								{
									iErr = SetTableRowAttribute (GiPanel, PANEL_MODE_TABLE_PRESSION, iOldStep, ATTR_USE_LABEL_TEXT, 1);
									sprintf(sLabel, "%d", iOldStep);
									iErr = SetTableRowAttribute (GiPanel, PANEL_MODE_TABLE_PRESSION, iOldStep, ATTR_LABEL_POINT_SIZE, 11);
									iErr = SetTableRowAttribute (GiPanel, PANEL_MODE_TABLE_PRESSION, iOldStep, ATTR_LABEL_TEXT, sLabel);
									iErr = SetTableRowAttribute (GiPanel, PANEL_MODE_TABLE_PRESSION, iOldStep, ATTR_LABEL_BOLD, 0);
								}

								iErr = SetTableRowAttribute (GiPanel, PANEL_MODE_TABLE_PRESSION, iIndexStepPression, ATTR_USE_LABEL_TEXT, 1);
								sprintf(sLabel, ">%d>", iIndexStepPression);
								iErr = SetTableRowAttribute (GiPanel, PANEL_MODE_TABLE_PRESSION, iIndexStepPression, ATTR_LABEL_POINT_SIZE, 15);
								iErr = SetTableRowAttribute (GiPanel, PANEL_MODE_TABLE_PRESSION, iIndexStepPression, ATTR_LABEL_TEXT, sLabel);
								iErr = SetTableRowAttribute (GiPanel, PANEL_MODE_TABLE_PRESSION, iIndexStepPression, ATTR_LABEL_BOLD, 1);

								iOldStep = iIndexStepPression;
							}
						}

						if (iIndexStepPression <= GstTabPression.iNbElmts)
						{
							// Calcul de la pression à programmer
							if (((dX2Config - dX1Config) > -0.001) && ((dX2Config - dX1Config) < 0.001))
							{
								SetGdPressionAProg(dY2Config);
							}
							else
							{
								dPente = (dY2Config - dY1Config) / (dX2Config - dX1Config);
								dCste = dY1Config - (dPente * dX1Config);

								SetGdPressionAProg(dPente * dDureeEcouleeCycle + dCste);
							}

							dDureeEcouleeCmde = (double)((clock() - iTimeDepCmde)) / ((double)CLOCKS_PER_SEC);

							// La commande de la pression se fait seulement à la vitesse définie dans le fichier de configuration
							if(dDureeEcouleeCmde > dDelayCmdPress)
							{
								// Programmation de la pression
								iErr = ProgPression(GetPointerToGptstConfig(), GetGdPressionAProg(), GetGiUnitPression());
								ReleasePointerToGptstConfig();

								iTimeDepCmde = clock();
							}

							dX1Relect		= dX2Relect;
							dY1Relect		= dY2Relect;
							dX2Relect		= dDureeEcouleeCycle;
							dY2Relect		= GetGdPression();

							// Affichage de la pression sur le graphique
							iHandle = PlotLine (GiPanel, PANEL_MODE_GRAPHE_VITESSE, dX1Relect, dY1Relect, dX2Relect, dY2Relect, iCOUL_PRESSION);
							SetPlotAttribute (GiPanel, PANEL_MODE_GRAPHE_VITESSE, iHandle, ATTR_PLOT_THICKNESS, 2);

							//modif carolina
							if(flag_seegraph == 1)
							{
								iHandle2 = PlotLine (GiGraphPanel, PANELGRAPH_GRAPH, dX1Relect, dY1Relect, dX2Relect, dY2Relect, iCOUL_PRESSION);
								SetPlotAttribute (GiGraphPanel, PANELGRAPH_GRAPH, iHandle2, ATTR_PLOT_THICKNESS, 2);
								if(flag_seegraph != 1)
								{
									iHandle2 = 0;
								}
							}

							iIndexMeas++;
						}

						// Lecture de la pression
						if((!GetGiStartCycleVitesse()) && GetGbEndCycleVitesse ())
						{
							LectEtAffichSpeedAndPress((!GetGiStartCycleVitesse()) && GetGbEndCycleVitesse (), (!GetGiStartCycleVitesse()) && GetGbEndCycleVitesse (), GiPanel, GiStatusPanel, GiGraphPanel,
													  GetPointerToGptstConfig(), &fVitesse, &dPression, GetGiUnitVitesse(), GetGiUnitPression(),
													  &iErrUsb6008, &iErrVaria);
							ReleasePointerToGptstConfig();

							SetGiErrUsb6008(iErrUsb6008);
							SetGiErrVaria(iErrVaria);
						}

						// Mise à jour des variables globales
						//-------------------------------------
						if((!GetGiStartCycleVitesse()) && GetGbEndCycleVitesse ())
							SetGdPression(dPression);

						SetGiSelectionHandlePression(iSelectionHandlePression);
						SetGdDurEcoulEssaiPress(dDureeEcouleeEssai);

						if(iIndexStepPression <= GstTabPression.iNbElmts)
							SetGiIndexStepPression(iIndexStepPression);

						if(iNumCycle <= iNbCyclesPrevus)
							SetGiIndexCyclePression(iNumCycle);
						//------------------------------------

						ProcessSystemEvents();
						dTime = Timer();
						SyncWait(dTime, dDELAY_THREAD_PRESS);
					}
					while((GetGiStartCyclePression()) && (iIndexStepPression <= GstTabPression.iNbElmts));

					if (iOldStep != 0)
					{
						iErr = SetTableRowAttribute (GiPanel, PANEL_MODE_TABLE_PRESSION, iOldStep, ATTR_USE_LABEL_TEXT, 1);
						sprintf(sLabel, "%d", iOldStep);
						iErr = SetTableRowAttribute (GiPanel, PANEL_MODE_TABLE_PRESSION, iOldStep, ATTR_LABEL_POINT_SIZE, 11);
						iErr = SetTableRowAttribute (GiPanel, PANEL_MODE_TABLE_PRESSION, iOldStep, ATTR_LABEL_TEXT, sLabel);
						iErr = SetTableRowAttribute (GiPanel, PANEL_MODE_TABLE_PRESSION, iOldStep, ATTR_LABEL_BOLD, 0);
					}

					// Remise à zéro du temps des pressions
					SetGbRazTimePress(1);

					if (GetGiStartCyclePression() == 1)
						// Incrémentation du numéro de cycle en cours
						iNumCycle++;

					if (iNumCycle <= iNbCyclesPrevus)
					{
						// Affichage du numéro de cycle en cours
						SetCtrlVal (GiPanel, PANEL_MODE_NUM_CYCLE_PRESSION, iNumCycle);

						/*if (GetGiStartCyclePression() == 1)
							// Effacement et tracé du graphique
							//GstGrapheMajGraphePres(GiPanel, PANEL_MODE_TABLE_PRESSION, PANEL_MODE_GRAPHE_VITESSE, 1, GstTabPression.dDuree, GstTabPression.dPress, GstTabPression.iNbElmts, iNumCycle, iCOUL_CONS_PRESSION, 1);
						//GstGrapheMajGraphePres(GiGraphPanel, PANEL_MODE_TABLE_PRESSION, PANELGRAPH_GRAPH, 1, GstTabPression.dDuree, GstTabPression.dPress, GstTabPression.iNbElmts, iNumCycle, iCOUL_CONS_PRESSION, 1);
						*/
					}

					ProcessSystemEvents();
					dTime = Timer();
					SyncWait(dTime, dDELAY_THREAD_PRESS);
				}
				while((iNumCycle <= iNbCyclesPrevus) && (GetGiStartCyclePression() == 1));

				SetGiTimeDepartEssaiPress(0);

				// Remise à zéro de la pression en fin de cycle
				SetGdPressionAProg(0.0);
				SetGiErrUsb6008(ProgPression(GetPointerToGptstConfig(), GetGdPressionAProg(), GetGiUnitPression()));
				ReleasePointerToGptstConfig();

				GetCtrlAttribute (GiPanel, PANEL_MODE_BUT_START_PRESSION, 	ATTR_DIMMED, 	&bDimmed1);
				GetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VIT_PRESS, ATTR_DIMMED, 	&bDimmed2);
				SetCtrlAttribute (GiPanel, PANEL_MODE_BUT_START_PRESSION, 	ATTR_DIMMED,  	1);
				SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VIT_PRESS, ATTR_DIMMED, 	1);

				// Attente de remise à zéro de la pression
				iNbEssais = 0;
				do
				{
					// Lecture de la pression
					if((!GetGiStartCycleVitesse()) && GetGbEndCycleVitesse ())
					{
						LectEtAffichSpeedAndPress((!GetGiStartCycleVitesse()) && GetGbEndCycleVitesse (), (!GetGiStartCycleVitesse()) && GetGbEndCycleVitesse (), GiPanel, GiStatusPanel,GiGraphPanel,
												  GetPointerToGptstConfig(), &fVitesse, &dPression, GetGiUnitVitesse(), GetGiUnitPression(),
												  &iErrUsb6008, &iErrVaria);
						ReleasePointerToGptstConfig();
						SetGiErrUsb6008(iErrUsb6008);
						SetGiErrVaria(iErrVaria);
					}

					if((!GetGiStartCycleVitesse()) && GetGbEndCycleVitesse ())
						SetGdPression(dPression);

					iNbEssais++;
					dTime = Timer();
					SyncWait(dTime, dDELAY_THREAD_PRESS);
					ProcessSystemEvents();
				}
				while (((GetGdPression() > ConvLimPression(dMAX_PRESS_START_TEST, GetGiUnitPression())) && (iNbEssais < 30)) && (!GetGbQuitter()));

				SetCtrlAttribute (GiPanel, PANEL_MODE_BUT_START_PRESSION, ATTR_DIMMED,  	bDimmed1);
				SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VIT_PRESS, ATTR_DIMMED, 	bDimmed2);

				SetGiStartCyclePression(0);
				// Fin du cycle
				iErr = GstIhmCycles(GiPanel, GetGiStartVitPress(), GetGiStartCycleVitesse(), GetGiStartCyclePression());

				SetGbEndCyclePression(1);
			}
		}
		else
		{
			//--------- MODE MANUEL --------
			dX1Config 		= 0.0;
			dY1Config 		= 0.0;
			dX2Config 		= 0.0;
			dY2Config 		= 0.0;

			dX1Relect		= 0.0;
			dY1Relect		= 0.0;
			dX2Relect		= 0.0;
			dY2Relect		= 0.0;

			dX1RelectCons	= 0.0;
			dY1RelectCons	= 0.0;
			dX2RelectCons	= 0.0;
			dY2RelectCons	= 0.0;

			dPente 			= 0.0;
			dCste			= 0.0;

			dMin			= 0.0;
			dMax			= 0.0;

			dLastDureeEcouleeEssai = 0.0;

			iTimeDepCmde	= 0;

			SetGdPressionAProg(0.0);
			SetGiIndexStepPression(0);
			SetGiIndexCyclePression(0);

			if (GetGiStartCyclePression())
			{
				SetGbEndCyclePression(0);
				//------- Modif du 15/11/2010 par C.BERENGER ------ DEBUT ------------
				SetAxisScalingMode (GiPanel, PANEL_MODE_GRAPHE_PRESSION, VAL_BOTTOM_XAXIS, VAL_AUTOSCALE, 0.0, 100.0);
				//------- Modif du 15/11/2010 par C.BERENGER ------ FIN ------------

				SetGiIndexStepPression(0);
				SetGiIndexCyclePression(0);

				// Reset du graphique
				DeleteGraphPlot (GiPanel, PANEL_MODE_GRAPHE_PRESSION, -1, VAL_IMMEDIATE_DRAW);

				//modif carolina
				if(flag_seegraph == 1)
				{
					SetGbEndCyclePression(0);
					SetAxisScalingMode (GiGraphPanel, PANELGRAPH_GRAPH, VAL_BOTTOM_XAXIS, VAL_AUTOSCALE, 0.0, 100.0);
					SetGiIndexStepPression(0);
					SetGiIndexCyclePression(0);
					DeleteGraphPlot (GiGraphPanel, PANELGRAPH_GRAPH, -1, VAL_IMMEDIATE_DRAW);
				}

				// Temps de départ
				SetGiTimeDepartEssaiPress(clock());
				iTimeDepartRampe = clock();

				// Si le cycle de pression est lancé
				do
				{
					// Lecture de la valeur de pression à programmer
					GetCtrlVal (GiPanel, PANEL_MODE_SAISIE_PRESSION_MANU, &dPressionPrevue);

					if (dY2Config != dPressionPrevue)
					{
						GetCtrlVal (GiPanel, PANEL_MODE_SAISIE_DUREE_PRES_MAN, &dDureePrevue);

						dX1Config = 0.0;
						dX2Config = dDureePrevue;
						dY1Config = dY2RelectCons;//dY2Config;
						dY2Config = dPressionPrevue;

						// Calcul de la pression à programmer
						if (((dX2Config - dX1Config) > -0.001) && ((dX2Config - dX1Config) < 0.001))
						{
							dPente = 0.0;
							dCste = 0.0;
							SetGdPressionAProg(dY2Config);
						}
						else
						{
							dPente = (dY2Config - dY1Config) / (dX2Config - dX1Config);
							dCste = dY1Config - (dPente * dX1Config);
						}

						// Mise à zéro de la durée écoulée
						dDureeEcouleeCycle = 0.0;
						// Temps de départ
						iTimeDepartRampe = clock();
					}

					dDureeEcouleeRampe = (double)((clock() - iTimeDepartRampe)) / ((double)CLOCKS_PER_SEC);

					// Rampe
					if (dDureeEcouleeRampe <= dDureePrevue)
					{
						// Calcul de la pression à programmer
						SetGdPressionAProg(dPente * dDureeEcouleeRampe + dCste);
					}
					else
						// Palier
						SetGdPressionAProg(dPressionPrevue);


					dDureeEcouleeCmde = (double)((clock() - iTimeDepCmde)) / ((double)CLOCKS_PER_SEC);

					// La commande de la pression se fait seulement à la vitesse définie dans le fichier de configuration
					if(dDureeEcouleeCmde > dDelayCmdPress)
					{
						// Programmation de la pression
						iErr = ProgPression(GetPointerToGptstConfig(), GetGdPressionAProg(), GetGiUnitPression());
						ReleasePointerToGptstConfig();
						iTimeDepCmde = clock();
					}

					// Calcul de la durée de l'essai
					dDureeEcouleeEssai = (double)((clock() - GetGiTimeDepartEssaiPress())) / ((double)CLOCKS_PER_SEC);
					//Affichage de la durée de l'essai à l'écran
					sFORMAT_TIME_PRESS((int)dDureeEcouleeEssai, sTpsEcoule)

					SetCtrlVal (GiPanel, PANEL_MODE_AFFICH_TPS_PRESS, sTpsEcoule);

					// Lecture de la pression
					if((!GetGiStartCycleVitesse()) && GetGbEndCycleVitesse ())
					{
						LectEtAffichSpeedAndPress((!GetGiStartCycleVitesse()) && GetGbEndCycleVitesse (), (!GetGiStartCycleVitesse()) && GetGbEndCycleVitesse (), GiPanel, GiStatusPanel,GiGraphPanel,
												  GetPointerToGptstConfig(), &fVitesse, &dPression, GetGiUnitVitesse(), GetGiUnitPression(),
												  &iErrUsb6008, &iErrVaria);
						ReleasePointerToGptstConfig();
						SetGiErrUsb6008(iErrUsb6008);
						SetGiErrVaria(iErrVaria);
					}

					// Mise à jour des variables globales
					//-------------------------------------
					if((!GetGiStartCycleVitesse()) && GetGbEndCycleVitesse ())
						SetGdPression(dPression);

					SetGdDurEcoulEssaiPress(dDureeEcouleeEssai);
					//------------------------------------


					// Calcul de la consigne de pression sur le graphique
					dX1RelectCons	= dX2RelectCons;
					dY1RelectCons	= dY2RelectCons;
					dX2RelectCons	= dDureeEcouleeEssai;
					dY2RelectCons	= GetGdPressionAProg();

					// Affichage de la consigne de pression sur le graphique
					iHandle = PlotLine (GiPanel, PANEL_MODE_GRAPHE_PRESSION, dX1RelectCons, dY1RelectCons, dX2RelectCons, dY2RelectCons, iCOUL_CONS_PRESSION);
					SetPlotAttribute (GiPanel, PANEL_MODE_GRAPHE_PRESSION, iHandle, ATTR_PLOT_THICKNESS, 1);

					//modif carolina
					if(flag_seegraph == 1)
					{
						iHandle2 = PlotLine (GiGraphPanel, PANELGRAPH_GRAPH, dX1RelectCons, dY1RelectCons, dX2RelectCons, dY2RelectCons, iCOUL_CONS_PRESSION);
						SetPlotAttribute (GiGraphPanel, PANELGRAPH_GRAPH, iHandle2, ATTR_PLOT_THICKNESS, 1);
						if(flag_seegraph != 1)
						{
							iHandle2 = 0;
						}
					}

					// Calcul de la relecture de pression sur le graphique
					dX1Relect	= dX2Relect;
					dY1Relect	= dY2Relect;
					dX2Relect	= dDureeEcouleeEssai;
					dY2Relect	= GetGdPression();

					// Affichage de la relecture de pression sur le graphique
					iHandle = PlotLine (GiPanel, PANEL_MODE_GRAPHE_PRESSION, dX1Relect, dY1Relect, dX2Relect, dY2Relect, iCOUL_PRESSION);
					SetPlotAttribute (GiPanel, PANEL_MODE_GRAPHE_PRESSION, iHandle, ATTR_PLOT_THICKNESS, 2);

					//modif carolina
					if(flag_seegraph == 1)
					{
						iHandle2 = PlotLine (GiGraphPanel, PANELGRAPH_GRAPH, dX1Relect, dY1Relect, dX2Relect, dY2Relect, iCOUL_PRESSION);
						SetPlotAttribute (GiGraphPanel, PANELGRAPH_GRAPH, iHandle2, ATTR_PLOT_THICKNESS, 2);
						if(flag_seegraph != 1)
						{
							iHandle2 = 0;
						}
					}

					//------- Modif du 15/11/2010 par C.BERENGER ------ DEBUT ------------
					if(dY2RelectCons < dMin) dMin = dY2RelectCons;
					if(dY2RelectCons > dMax) dMax = dY2RelectCons;
					if(dY2Relect < dMin) dMin = dY2Relect;
					if(dY2Relect > dMax) dMax = dY2Relect;

					if(dDureeEcouleeEssai > dPlageAffichManu)
						if(dDureeEcouleeEssai - dLastDureeEcouleeEssai > dPlageAffichManu)
						{
							// Reset du graphique
							DeleteGraphPlot (GiPanel, PANEL_MODE_GRAPHE_PRESSION, -1, VAL_IMMEDIATE_DRAW);
							SetAxisScalingMode (GiPanel, PANEL_MODE_GRAPHE_PRESSION, VAL_BOTTOM_XAXIS, VAL_MANUAL, dDureeEcouleeEssai, dDureeEcouleeEssai + dPlageAffichManu);
							// Points pour garder l'échelle précédente car autoscale vertical
							PlotPoint (GiPanel, PANEL_MODE_GRAPHE_PRESSION, dDureeEcouleeEssai, dMax, VAL_NO_POINT, VAL_TRANSPARENT);
							PlotPoint (GiPanel, PANEL_MODE_GRAPHE_PRESSION, dDureeEcouleeEssai, dMin, VAL_NO_POINT, VAL_TRANSPARENT);

							if(flag_seegraph == 1 )
							{
								// Reset du graphique
								DeleteGraphPlot (GiGraphPanel, PANELGRAPH_GRAPH, -1, VAL_IMMEDIATE_DRAW);
								SetAxisScalingMode (GiGraphPanel, PANELGRAPH_GRAPH, VAL_BOTTOM_XAXIS, VAL_MANUAL, dDureeEcouleeEssai, dDureeEcouleeEssai + dPlageAffichManu);
								// Points pour garder l'échelle précédente car autoscale vertical
								PlotPoint (GiGraphPanel, PANELGRAPH_GRAPH, dDureeEcouleeEssai, dMax, VAL_NO_POINT, VAL_TRANSPARENT);
								PlotPoint (GiGraphPanel, PANELGRAPH_GRAPH, dDureeEcouleeEssai, dMin, VAL_NO_POINT, VAL_TRANSPARENT);
							}
							dLastDureeEcouleeEssai =  dDureeEcouleeEssai;


						}
					//------- Modif du 15/11/2010 par C.BERENGER ------ FIN ------------

					if(GetGiStartCyclePression() == 0)
					{
						SetGiTimeDepartEssaiPress(0);
						// Reset de la pression
						SetGiErrUsb6008(ProgPression(GetPointerToGptstConfig(), 0.0, GetGiUnitPression()));
						ReleasePointerToGptstConfig();
					}

					dTime = Timer();
					SyncWait(dTime, dDELAY_THREAD_PRESS);
					ProcessSystemEvents();
				}
				while(GetGiStartCyclePression());
				// Remise à zéro de la pression en fin de cycle
				SetGdPressionAProg(0.0);
				SetGiErrUsb6008(ProgPression(GetPointerToGptstConfig(), GetGdPressionAProg(), GetGiUnitPression()));
				ReleasePointerToGptstConfig();

				GetCtrlAttribute (GiPanel, PANEL_MODE_BUT_START_PRESSION, 	ATTR_DIMMED,   	&bDimmed1);
				GetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VIT_PRESS, ATTR_DIMMED, 	&bDimmed2);
				SetCtrlAttribute (GiPanel, PANEL_MODE_BUT_START_PRESSION,  	ATTR_DIMMED,   	1);
				SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VIT_PRESS, ATTR_DIMMED, 	1);

				// Attente de remise à zéro de la pression
				iNbEssais = 0;
				do
				{
					// Lecture de la pression
					if((!GetGiStartCycleVitesse()) && GetGbEndCycleVitesse ())
					{
						LectEtAffichSpeedAndPress((!GetGiStartCycleVitesse()) && GetGbEndCycleVitesse (), (!GetGiStartCycleVitesse()) && GetGbEndCycleVitesse (), GiPanel, GiStatusPanel,GiGraphPanel,
												  GetPointerToGptstConfig(), &fVitesse, &dPression, GetGiUnitVitesse(), GetGiUnitPression(),
												  &iErrUsb6008, &iErrVaria);
						ReleasePointerToGptstConfig();
						SetGiErrUsb6008(iErrUsb6008);
						SetGiErrVaria(iErrVaria);
					}

					if((!GetGiStartCycleVitesse()) && GetGbEndCycleVitesse ())
						SetGdPression(dPression);

					iNbEssais++;

					dTime = Timer();
					SyncWait(dTime, dDELAY_THREAD_PRESS);
					ProcessSystemEvents();
				}
				while (((GetGdPression() > ConvLimPression(dMAX_PRESS_START_TEST, GetGiUnitPression())) && (iNbEssais < 30)) && (!GetGbQuitter()));

				SetCtrlAttribute (GiPanel, PANEL_MODE_BUT_START_PRESSION, ATTR_DIMMED,   bDimmed1);
				SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VIT_PRESS, ATTR_DIMMED, bDimmed2);

				SetGbEndCyclePression(1);
			}
		}

		dTime = Timer();
		SyncWait(dTime, dDELAY_THREAD_PRESS);
		ProcessSystemEvents();
	}

	return(0);
}

// DEBUT ALGO
//***************************************************************************
// void ConvertPointToVirg(char *sMsgToConvert)
//***************************************************************************
//  - char *sMsgToConvert	: Chaîne de caractères à traiter
//
//  - Remplacement des points par des virgules dans la chaîne de caractères
//
//  - Aucun
//***************************************************************************
// FIN ALGO
void ConvertPointToVirg(char *sMsgToConvert)
{
	char *ptChar;

	ptChar = strchr(sMsgToConvert, '.');
	while(ptChar!=NULL)
	{
		*ptChar = ',';
		ptChar = strchr(sMsgToConvert, '.');
	}
}

// DEBUT ALGO
//***************************************************************************
//static int CVICALLBACK ThreadAcquisitions (void *functionData)
//***************************************************************************
//  - functionData: pointeur sur des données
//
//  - Thread d'acquisition de la vitesse moteur et de la pression
//
//  - 0 si OK, sinon 1
//***************************************************************************
// FIN ALGO
//static int CVICALLBACK ThreadAcquisitions (void *functionData)
int CVICALLBACK ThreadAcquisitions (int reserved, int theTimerId, int event,
									void *callbackData, int eventData1,
									int eventData2)
{
#define iMAX_DATA_CLOSE_FILE	50
#define dDELAY_THREAD_ACQUIS 	0.2
	char sTitle[500]="";
	float fVitesse;
	double dPression;
	//double dTime;
	static char sMsg[2000];
	double dTimeElapsed;
	//double dDelay;
	double dDureeEcouleeRecord;
	char sDateHeure[50];
	char sResult[5000];
	char sValSpeed[100];
	char sValPress[100];
	char sFileName[200];
	int iIndexMeasSpeed	= 1;
	int iIndexMeasPress	= 1;
	int iErr;
	int iTimeDepRecord;
	//int iIndexCycle;
	//int iLastStatusVaria= -1;
	//int iLastStatusUsb 	= -1;
	int iErrUsb6008 	= 0;
	int iErrVaria		= 0;
	int iHandleFileResult = 0;
	int iIndexCloseFile = 0;
	int iSaveData=1;
	BOOL bClose;
	stConfig *ptstConfig;
	stTabFile stLocalTabFile[1];

	SetGiAffMsg(0);

	ptstConfig = GetPointerToGptstConfig();
	iSaveData = ptstConfig->iSaveResults;
	ReleasePointerToGptstConfig();


	if (event == EVENT_TIMER_TICK)
	{
		//while (!GetGbQuitter()) {
		// Mise à jour du status de la communication avec le variateur
		// Lecture et affichage de la vitesse et de la pression
		if ((!GetGiStartCycleVitesse()) && (!GetGiStartCyclePression()) && (GetGbEndCycleVitesse ()) && (GetGbEndCyclePression()))
		{
			LectEtAffichSpeedAndPress((!GetGiStartCycleVitesse()) && GetGbEndCycleVitesse (), (!GetGiStartCyclePression()) && GetGbEndCyclePression(), GiPanel, GiStatusPanel,GiGraphPanel,
									  GetPointerToGptstConfig(), &fVitesse, &dPression, GetGiUnitVitesse(), GetGiUnitPression(),
									  &iErrUsb6008, &iErrVaria);
			ReleasePointerToGptstConfig();
			//----------------------------
			SetGiErrUsb6008(iErrUsb6008);
			SetGiErrVaria(iErrVaria);
			SetGfVitesse (fVitesse);
			SetGdPression(dPression);
			//----------------------------
		}

		iErr = 0;

		// Enregistrement des résultats si au moins un cycle (vitesse ou pression) est lancé
		if ((GetGiStartCycleVitesse() || GetGiStartCyclePression()) && (iSaveData==1))
		{
			// Si le fichier n'a pas été créé
			//-------------------------------
			if (GetGiHandleFileResult() < 0)
			{
				if(iErr == 0)
				{
					// Création du répertoire résultats
					iErr = CreateRepResultIfNeeded();
					CHECK_ERROR(iErr, "Error", sMSG_ERR_CREAT_RES_DIR)
				}

				if(iErr == 0)
				{
					GstFilesGetStringDateHeure(sDateHeure);

					if ((GetGiMode() == iMODE_AUTOMATIQUE)||(GetGiMode() == iMODE_RUN))
					{
						sprintf(sFileName, "%s_Auto_", sDateHeure);
						// Création du fichier résultats et copie du fichier de configuration
						iErr = GstFilesCreateResultFiles(1, GsPathConfigFile, &stLocalTabFile[0], GsPathResult, sFileName, sEXT_FICHIERS, &iHandleFileResult, sMsg);

						//---------------------------------------
						SetGiHandleFileResult(iHandleFileResult);
						//---------------------------------------
					}
					else
					{
						sprintf(sFileName, "%s_Man_", sDateHeure);
						// Création du fichier résultats et copie du fichier de configuration
						// Init Dll
						iErr = GstFilesCreateResultFiles(0, GsPathConfigFile, &stLocalTabFile[0], GsPathResult, sFileName, sEXT_FICHIERS, &iHandleFileResult, sMsg);

						//---------------------------------------
						SetGiHandleFileResult(iHandleFileResult);
						//---------------------------------------
					}
					CHECK_ERROR(iErr,"Error creating result files", sMsg)
				}

				sprintf(sResult, sMSG_ENT_MEAS_SPEED);

				switch(GetGiUnitVitesse())
				{
					case iUNIT_KM_H:
						strcat(sResult, sAFFICH_VITESSE_KM_H);
						break;
					case iUNIT_G:
						strcat(sResult, sAFFICH_VITESSE_G);
						break;
					case iUNIT_TRS_MIN:
						strcat(sResult, sAFFICH_VITESSE_TRS_MIN);
						break;
				}

				strcat(sResult, sMSG_ENT_MEAS_PRESS);

				switch(GetGiUnitPression())
				{
					case iUNIT_KPA:
						strcat(sResult, sAFFICH_PRES_KPA);
						break;
					case iUNIT_BAR:
						strcat(sResult, sAFFICH_PRES_BARS);
						break;
					case iUNIT_MBAR:
						strcat(sResult, sAFFICH_PRES_MBAR);
						break;
					case iUNIT_PSI:
						strcat(sResult, sAFFICH_PRES_PSI);
						break;
				}
				strcat(sResult, ";");

				if(iErr == 0)
				{
					// Ajout d'une ligne dans le fichier résultats
					iErr = GstFilesAddResultFiles(&stLocalTabFile[0], iHandleFileResult, sResult, 1, sMsg);
					//---- Modification du 04/04/2013 par CB --- DEBUT ----------
					iErr = 0;
					//---- Modification du 04/04/2013 par CB --- FIN ----------
					CHECK_ERROR(iErr, sMSG_ERR_ADD_RES_LINE, sMsg)
				}
			}
			//--------------------------------------------


			//---------- Resultats -----------------------
			//dDureeEcouleeRecord = (double)((clock() - iTimeDepRecord)) / ((double)CLOCKS_PER_SEC);
			dDureeEcouleeRecord = *((double*)eventData1);

			// Si l'on doit réaliser l'enregistrement d'une mesure
			//if(dDureeEcouleeRecord > dDelay){
			// Réinit du temps entre deux enregistrements
			iTimeDepRecord = clock();
			strcpy(sResult, "");

			// Enregistrement des données seulement après la deuxième mesure
			if ((iIndexMeasSpeed > 0) || (iIndexMeasPress > 0))
			{
				// Ajout de la mesure de vitesse si nécessaire
				if(GetGiStartCycleVitesse())
				{
					if (iIndexMeasSpeed > 0)
					{
						sprintf(sValSpeed, iFORMAT_SPEED_RESULT, iIndexMeasSpeed,  GetGdDurEcoulEssaiSpeed(), GetGiIndexCycleVitesse(), GetGiIndexStepVitesse(), GetGdVitesseAProg() , GetGfVitesse ());
						strcat(sResult, sValSpeed);
					}
				}
				else
					strcat(sResult, ";;;;;;");

				// Ajout de la mesure de pression si nécessaire
				if(GetGiStartCyclePression())
				{
					if (iIndexMeasPress > 0)
					{
						if(GetGiStartCycleVitesse())
							dTimeElapsed = GetGdDurEcoulEssaiSpeed();
						else
							dTimeElapsed = (double)((clock() - GetGiTimeDepartEssaiPress())) / ((double)CLOCKS_PER_SEC);
						sprintf(sValPress, iFORMAT_PRESS_RESULT, iIndexMeasPress,  dTimeElapsed, GetGiIndexCyclePression(), GetGiIndexStepPression(), (float)GetGdPressionAProg(), (float)GetGdPression());
						strcat(sResult, sValPress);
					}
				}
				else
					strcat(sResult, "<->;;;;;;");

				if(iErr == 0)
				{
					// Conversion des points en virgules pour traitement sous Excel
					ConvertPointToVirg(sResult);

					/*if(iIndexCloseFile > iMAX_DATA_CLOSE_FILE ){
						bClose = 1;
						iIndexCloseFile = 0;
					}
					else
						bClose = 0;
					*/
					bClose = 1;

					// Ajout d'une ligne dans le fichier résultats
					//iErr = GstFilesAddResultFiles(&stLocalTabFile[0], iHandleFileResult, sResult, bClose, sMsg);
					//---- Modification du 04/04/2013 par CB --- DEBUT ----------
					iErr = 0;
					//---- Modification du 04/04/2013 par CB --- FIN ----------
					// Incrémentation du nombre d'écritures fichier
					iIndexCloseFile++;
					CHECK_ERROR(iErr, sMSG_ERR_ADD_RES_LINE, sMsg)
				}
			}

			//----- VITESSE -----
			if(GetGiStartCycleVitesse())
			{
				// Incrémentation du numéro d'index des mesures
				iIndexMeasSpeed++;
			}

			//----- PRESSION -----
			if(GetGiStartCyclePression())
			{
				// Incrémentation du numéro d'index des mesures
				iIndexMeasPress++;
			}
			//}
		}
		else
		{
			// Remise à zéro de l'index de chaque mesure
			iIndexMeasSpeed 	= 0;
			iIndexMeasPress 	= 0;

			// Fermeture du fichier résultats si nécessaire
			//if ((GetGiHandleFileResult() >= 0) && (iSaveData==1))
			//	GstFilesCloseResultFile(&stLocalTabFile[0], GetGiHandleFileResult(), sMsg);

			GstFilesResetAll(&stLocalTabFile[0]);

			// Remise à zéro du handle de fichier résultats
			SetGiHandleFileResult(-1);
			// Arrêt de la tâche d'acquisition
			SetAsyncTimerAttribute (GiThreadAcquisitions, ASYNC_ATTR_ENABLED, 0);
		}

		//ProcessSystemEvents();
		//dTime = Timer();
		//SyncWait(dTime, dDELAY_THREAD_ACQUIS);
	}
	//else
	/* We will get an EVENT_DISCARD when the timer is destroyed. */
	//	if (event == EVENT_DISCARD)
	//  	g_timerId = 0;

	return(0);
}

// DEBUT ALGO
//***************************************************************************
// int RetourEtatAttente(int *ptiPorte1Ouverte, int *ptiPorte2Ouverte, int *ptiCapotOuvert,
//						int *ptiBoucleSecOuv, int *ptiMoteurArret, int *pt&iArretUrgence)
//***************************************************************************
//  - int *ptiPorte1Ouverte		: Etat de la porte 1
//	  int *ptiPorte2Ouverte		: Etat de la porte 2
//	  int *ptiCapotOuvert 		: Etat du capot de protection
//	  int *ptiBoucleSecOuv		: Etat de la boucle de sécurité
//	  int *ptiMoteurArret		: Etat du moteur
//	  int *pt&iArretUrgence		: Etat du bouton d'arrêt d'urgence
//
//  - Fonction vérifiant si les conditions pour revenir à l'état d'attente
//	  sont remplies
//
//  - 1 s'il faut revenir à l'état d'attente, sinon 0
//***************************************************************************
// FIN ALGO
int RetourEtatAttente(int *ptiPorte1Ouverte, int *ptiPorte2Ouverte, int *ptiCapotOuvert,
					  int *ptiBoucleSecOuv, int *ptiMoteurArret, int *ptiArretUrgence,
					  int *ptiEtatCaptVib)
{

	// Initialisations
	*ptiPorte1Ouverte 	= iUSB6008_ETAT_PORTE_1_OUVERTE;
	*ptiPorte2Ouverte 	= iUSB6008_ETAT_PORTE_2_OUVERTE;
	*ptiCapotOuvert 	= iUSB6008_ETAT_CAPOT_SECURITE_OUVERT;
	*ptiBoucleSecOuv	= iUSB6008_ETAT_BOUCLE_SECURITE_OUVERTE;
	*ptiMoteurArret 	= !iUSB6008_ETAT_MOTEUR_ARRETE;
	*ptiArretUrgence 	= iUSB6008_ETAT_ARRET_URGENCE_ACTIF;
	*ptiEtatCaptVib 	= iUSB6008_ETAT_VIBRATIONS_DETECT_ACTIF;


	// Lecture des valeurs logiques
	SetGiErrUsb6008(Usb6008AcquisitionEtats (ptiPorte1Ouverte, ptiPorte2Ouverte, ptiCapotOuvert,
					ptiBoucleSecOuv, ptiMoteurArret, ptiArretUrgence,
					ptiEtatCaptVib));

	if (GetGiErrUsb6008())
	{
		return 1; // Une erreur s'est produite
	}

	// Mise à jour de l'écran de status si besoin
	if (GetGiPanelStatusDisplayed())
	{
		SetCtrlVal (GiStatusPanel, PANEL_STAT_DOOR_1, 			(*ptiPorte1Ouverte));
		SetCtrlVal (GiStatusPanel, PANEL_STAT_DOOR_2, 			(*ptiPorte2Ouverte));
		SetCtrlVal (GiStatusPanel, PANEL_STAT_SECURITY_HOOD, 	(*ptiCapotOuvert));
		SetCtrlVal (GiStatusPanel, PANEL_STAT_SECURITY_LOOP, 	(*ptiBoucleSecOuv));
		SetCtrlVal (GiStatusPanel, PANEL_STAT_ENGINE_RUNNING, 	!(*ptiMoteurArret));
		SetCtrlVal (GiStatusPanel, PANEL_STAT_EMERGENCY_STOP, 	(*ptiArretUrgence));
	}

	// Détermine s'il faut revenir à l'état de départ
	if(	(*ptiPorte1Ouverte 	== iUSB6008_ETAT_PORTE_1_OUVERTE) 			||
			(*ptiPorte2Ouverte 	== iUSB6008_ETAT_PORTE_2_OUVERTE) 			||
			(*ptiCapotOuvert 	== iUSB6008_ETAT_CAPOT_SECURITE_OUVERT) 	||
			(*ptiBoucleSecOuv 	== iUSB6008_ETAT_BOUCLE_SECURITE_OUVERTE)	||
			(*ptiMoteurArret 	!= iUSB6008_ETAT_MOTEUR_ARRETE) 			||
			(*ptiArretUrgence 	== iUSB6008_ETAT_ARRET_URGENCE_ACTIF)		||
			(*ptiEtatCaptVib 	== iUSB6008_ETAT_VIBRATIONS_DETECT_ACTIF))
		return  1;
	else
	{
		// Si l'écran de status n'est pas affiché, alors affichage de celui-ci.
		return 0;
	}
}

// DEBUT ALGO
//***************************************************************************
// static int CVICALLBACK ThreadGestionSecurite (void *functionData)
//***************************************************************************
//  - functionData: pointeur sur des données
//
//  - Thread de gestion de la sécurité de l'opérateur
//
//  - 1 si problème détecté, 0 sinon
//***************************************************************************
// FIN ALGO
static int CVICALLBACK ThreadGestionSecurite (void *functionData)
{
#define dDELAY_SECURITY_PROCESS 0.5
	char sMsg[500];
	int iPorte1Ouverte 	= iUSB6008_ETAT_PORTE_1_OUVERTE;
	int iPorte2Ouverte 	= iUSB6008_ETAT_PORTE_2_OUVERTE;
	int iCapotOuvert 	= iUSB6008_ETAT_CAPOT_SECURITE_OUVERT;
	int iBoucleSecOuv	= iUSB6008_ETAT_BOUCLE_SECURITE_OUVERTE;
	int iMoteurArret 	= iUSB6008_ETAT_MOTEUR_ARRETE;
	int iArretUrgence 	= iUSB6008_ETAT_ARRET_URGENCE_ACTIF;
	int iEtatCaptVib	= iUSB6008_ETAT_VIBRATIONS_DETECT_ACTIF;
	int iNbPannesSucc 	= 0;
	int iAffMsg		  	= 0;
	//int iPanelStatusDisplayed=0;
	BOOL bAffichMsgAttente		= 1;
	BOOL bAffichMsgAttElectLock	= 0;
	BOOL bAffichMsgAttBoucle	= 0;
	BOOL bAffichMsgAttDepEssai	= 0;
	BOOL bAffichMsgEssEnCours	= 0;
	float fVitesse;
	double dTime;
	int iLastStatusUsb = -1;
	int iLastStatusVaria= -1;


	while (!GetGbQuitter())
	{
		//-----------------------------------------------------------------------------------------------
		if (GetGiErrVaria() == 0)
		{
			if (iLastStatusVaria != GetGiErrVaria())
			{
				SetCtrlVal (GiStatusPanel, PANEL_STAT_VARIATOR_COMM, 1); //OK
				SetCtrlAttribute (GiStatusPanel, PANEL_STAT_SPEED, 			ATTR_DIMMED, 0);
				SetCtrlAttribute (GiStatusPanel, PANEL_STAT_ENGINE_RUNNING,	ATTR_DIMMED, 0);
				iLastStatusVaria = GetGiErrVaria();
			}
		}
		else
		{
			if (iLastStatusVaria != GetGiErrVaria())
			{
				SetCtrlVal (GiStatusPanel, PANEL_STAT_VARIATOR_COMM, 0); // ERREUR
				SetCtrlAttribute (GiStatusPanel, PANEL_STAT_SPEED, 			ATTR_DIMMED, 1);
				SetCtrlAttribute (GiStatusPanel, PANEL_STAT_ENGINE_RUNNING,	ATTR_DIMMED, 1);
				iLastStatusVaria = GetGiErrVaria();
			}
		}

		// Mise à jour du status avec le module USB
		if (GetGiErrUsb6008() == 0)
		{
			if (iLastStatusUsb != GetGiErrUsb6008())
			{
				SetCtrlVal (GiStatusPanel, PANEL_STAT_USB6008_COMM, 1); //OK

				SetCtrlAttribute (GiStatusPanel, PANEL_STAT_DOOR_1, 		ATTR_DIMMED, 0);
				SetCtrlAttribute (GiStatusPanel, PANEL_STAT_DOOR_2, 		ATTR_DIMMED, 0);
				SetCtrlAttribute (GiStatusPanel, PANEL_STAT_SECURITY_HOOD, 	ATTR_DIMMED, 0);
				SetCtrlAttribute (GiStatusPanel, PANEL_STAT_SECURITY_LOOP, 	ATTR_DIMMED, 0);
				SetCtrlAttribute (GiStatusPanel, PANEL_STAT_EMERGENCY_STOP, ATTR_DIMMED, 0);
				SetCtrlAttribute (GiStatusPanel, PANEL_STAT_PRESSURE, 		ATTR_DIMMED, 0);

				iLastStatusUsb = GetGiErrUsb6008();
			}
		}
		else
		{
			if (iLastStatusUsb != GetGiErrUsb6008())
			{
				SetCtrlVal (GiStatusPanel, PANEL_STAT_USB6008_COMM, 0);

				SetCtrlAttribute (GiStatusPanel, PANEL_STAT_DOOR_1, 		ATTR_DIMMED, 1);
				SetCtrlAttribute (GiStatusPanel, PANEL_STAT_DOOR_2, 		ATTR_DIMMED, 1);
				SetCtrlAttribute (GiStatusPanel, PANEL_STAT_SECURITY_HOOD, 	ATTR_DIMMED, 1);
				SetCtrlAttribute (GiStatusPanel, PANEL_STAT_SECURITY_LOOP, 	ATTR_DIMMED, 1);
				SetCtrlAttribute (GiStatusPanel, PANEL_STAT_EMERGENCY_STOP, ATTR_DIMMED, 1);
				SetCtrlAttribute (GiStatusPanel, PANEL_STAT_PRESSURE, 		ATTR_DIMMED, 1);

				char *confpath;
				char auxGsLogPath[500];
				strcpy(auxGsLogPath,  GsLogPath);
				confpath= strcat(auxGsLogPath,"\\USB_Error.txt");
				int f = OpenFile (confpath, VAL_WRITE_ONLY, VAL_APPEND, VAL_ASCII);


				int dd,mm,yy,h,m,s;

				if(GetSystemTime (&h, &m, &s) == 0 && GetSystemDate (&mm,&dd,&yy) == 0)
				{
					sprintf(auxGsLogPath,"USB Error encountered during execution: Date: %d-%d-%d, Time: %d-%d-%d", dd,mm,yy,h,m,s);

					WriteLine(f,auxGsLogPath,-1);
				}
				else
				{
					WriteLine(f,"USB Error encountered during execution! ",-1);
				}
				CloseFile(f);

				iLastStatusUsb = GetGiErrUsb6008();
			}
		}
		//-----------------------------------------------------------------------------------------------


		if(GetGiEtat() == iETAT_ATTENTE_AFFICH_IHM)
		{
			;
		}

		// Etat d'attente
		if(GetGiEtat() ==  iETAT_ATTENTE)
		{
			if(bAffichMsgAttente)
			{
				SetGiErrUsb6008(Usb6008CommandeVentilationMoteur (iUSB6008_CMDE_VENTILATION_MOTEUR_INACTIFS));
				fVitesse = GetGfVitesse();

				// Reset du chemnin vers le répertoire des résultats
				strcpy(GsPathResult, "");

				// Demande d'appui sur le bouton de la boucle de sécurité
				// Reset du chemnin vers le répertoire des résultats
				strcpy(GsPathResult, "");
				ModesAddMessageZM(sMSG_SECURE_BENCH);

				bAffichMsgAttente = 0;

				// Mise à jour du voyant d'autorisation de départ Essai
				SetCtrlVal (GiPanel, PANEL_MODE_START_ENABLE, 0);
				SetCtrlAttribute (GiPanel,PANEL_MODE_MODE_EXE, 		ATTR_DIMMED, 1);   
				SetCtrlVal (GiPanel, PANEL_MODE_START_ENABLE2, 0x00F64949);
				
				

				// Arrêt des cycles en cours
				SetGiStartCycleVitesse(0);
				SetGiStartCyclePression(0);
				SetGiStartVitPress(0);
				GstIhmCycles(GiPanel, GetGiStartVitPress(),  GetGiStartCycleVitesse(), GetGiStartCyclePression());

				// On interdit l'appui sur les boutons de départ essai
				SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VITESSE, ATTR_DIMMED, 	1);
				SetCtrlAttribute (GiPanel, PANEL_MODE_BUT_START_PRESSION, ATTR_DIMMED, 	1);
				SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VIT_PRESS, ATTR_DIMMED, 1);

				//SetCtrlAttribute (GiPanel, PANEL_MODE_BUT_START_RUN, ATTR_DIMMED, 1);

				// Arrêt de la tâche d'acquisition
				//SetAsyncTimerAttribute (GiThreadAcquisitions, ASYNC_ATTR_ENABLED, 0);
			}

			fVitesse = GetGfVitesse();

			// Autorisation de l'ouverture gâche si le moteur ne tourne pas et si la pression est assez basse
			if ((fabs(fVitesse) < dVIT_ABS_START_TEST) && (GetGdPression() < ConvLimPression(dMAX_PRESS_START_TEST, GetGiUnitPression())))
			{
				SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_OUV_GACHE, ATTR_DIMMED, 0);
			}
			else
			{
				// Si la gâche était acive, alors inactivation
				if (GetGbGacheActive())
				{
					SetGiErrUsb6008(Usb6008CommandeOuvertureGache (iUSB6008_CMDE_OUVERTURE_GACHE_INACTIVE));

					// S'il n'y a pas d'erreur
					if(GetGiErrUsb6008() == 0)
					{
						SetGbGacheActive(iGACHE_FERMEE);
						SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_OUV_GACHE, ATTR_LABEL_TEXT, "Open Electric Lock");
					}
				}

				SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_OUV_GACHE, ATTR_DIMMED, 1);
			}

			// Lecture des valeurs logiques
			RetourEtatAttente(&iPorte1Ouverte, &iPorte2Ouverte, &iCapotOuvert,
							  &iBoucleSecOuv, &iMoteurArret, &iArretUrgence, &iEtatCaptVib);

			if(	(iPorte1Ouverte != iUSB6008_ETAT_PORTE_1_OUVERTE) 			&&
					(iPorte2Ouverte != iUSB6008_ETAT_PORTE_2_OUVERTE) 			&&
					(iCapotOuvert 	!= iUSB6008_ETAT_CAPOT_SECURITE_OUVERT) 	&&
					(iMoteurArret 	== iUSB6008_ETAT_MOTEUR_ARRETE) 			&&
					(GetGdPression() 	< ConvLimPression(dMAX_PRESS_START_TEST, GetGiUnitPression()))				&&
					(GetGiErrVaria() 	== 0)									&&
					(GetGiErrUsb6008() 	== 0)									&&
					(iArretUrgence 	!= iUSB6008_ETAT_ARRET_URGENCE_ACTIF)		&&
					(iEtatCaptVib 	!=iUSB6008_ETAT_VIBRATIONS_DETECT_ACTIF))
			{

				if(GetGbGacheActive() != iGACHE_FERMEE)
				{
					// La gache doit être fermée pour lancer l'essai
					SetGiEtat(iETAT_DEM_CLOSE_ELECTRIC_LOCK);
					bAffichMsgAttElectLock = 1;
				}
				else
				{
					// Attente d'appui sur le bouton bleu
					SetGiEtat(iETAT_DEM_APP_BOUCLE_SECUR);
					bAffichMsgAttBoucle = 1;
				}
			}
		}


		// On demande l'appui sur le bouton de la boucle de sécurité
		if(GetGiEtat() == iETAT_DEM_CLOSE_ELECTRIC_LOCK)
		{
			// Autorisation de l'ouverture gâche si le moteur ne tourne pas et si la pression est assez basse
			fVitesse = GetGfVitesse();
			if ((fabs(fVitesse) < dVIT_ABS_START_TEST) && (GetGdPression() < ConvLimPression(dMAX_PRESS_START_TEST, GetGiUnitPression())))
				SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_OUV_GACHE, ATTR_DIMMED, 0);
			else
				SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_OUV_GACHE, ATTR_DIMMED, 1);

			// Lecture des valeurs logiques
			RetourEtatAttente(&iPorte1Ouverte, &iPorte2Ouverte, &iCapotOuvert,
							  &iBoucleSecOuv, &iMoteurArret, &iArretUrgence, &iEtatCaptVib);

			if(	(iPorte1Ouverte == iUSB6008_ETAT_PORTE_1_OUVERTE) 			||
					(iPorte2Ouverte == iUSB6008_ETAT_PORTE_2_OUVERTE) 			||
					(iCapotOuvert 	== iUSB6008_ETAT_CAPOT_SECURITE_OUVERT) 	||
					(iMoteurArret 	!= iUSB6008_ETAT_MOTEUR_ARRETE) 			||
					(GetGdPression() > ConvLimPression(dMAX_PRESS_START_TEST, GetGiUnitPression())) 					||
					(GetGiErrVaria() 	!= 0)									||
					(GetGiErrUsb6008() 	!= 0)									||
					(iArretUrgence 	== iUSB6008_ETAT_ARRET_URGENCE_ACTIF)		||
					(iEtatCaptVib 	==iUSB6008_ETAT_VIBRATIONS_DETECT_ACTIF))
			{

				// On cache le message d'attente d'appui sur le bouton de la boucle de sécurité
				SetCtrlAttribute (GiPanel, PANEL_MODE_MessageIndicateur, ATTR_VISIBLE, 0);
				bAffichMsgAttente = 1;
				SetGiEtat(iETAT_ATTENTE);
			}
			else
			{
				if(bAffichMsgAttElectLock)
				{
					// Demande d'appui sur le bouton de fermeture de la gâche (sur l'IHM)
					ModesAddMessageZM(sMSG_CLOSE_ELECTRIC_LOCK);

					ResetTextBox (GiPanel, PANEL_MODE_MessageIndicateur, sMSG_CLOSE_ELECTRIC_LOCK);

					// Passage au premier plan du message
					SetCtrlAttribute (GiPanel, PANEL_MODE_MessageIndicateur, ATTR_VISIBLE, 1);
					SetCtrlAttribute (GiPanel, PANEL_MODE_MessageIndicateur, ATTR_ZPLANE_POSITION, 0);

					bAffichMsgAttElectLock = 0;
				}

				if (GetGbGacheActive() == iGACHE_FERMEE)
				{
					bAffichMsgAttBoucle = 1;
					// On cache le message d'attente d'appui sur le bouton de la boucle de sécurité
					SetCtrlAttribute (GiPanel, PANEL_MODE_MessageIndicateur, ATTR_VISIBLE, 0);
					SetGiEtat(iETAT_DEM_APP_BOUCLE_SECUR);
				}
			}
		}


		// On demande l'appui sur le bouton de la boucle de sécurité
		if(GetGiEtat() ==  iETAT_DEM_APP_BOUCLE_SECUR)
		{
			// Autorisation de l'ouverture gâche si le moteur ne tourne pas et si la pression est assez basse
			fVitesse = GetGfVitesse();
			if ((fabs(fVitesse) < dVIT_ABS_START_TEST) && (GetGdPression() < ConvLimPression(dMAX_PRESS_START_TEST, GetGiUnitPression())))
				SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_OUV_GACHE, ATTR_DIMMED, 0);
			else
				SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_OUV_GACHE, ATTR_DIMMED, 1);

			// Lecture des valeurs logiques
			RetourEtatAttente(&iPorte1Ouverte, &iPorte2Ouverte, &iCapotOuvert,
							  &iBoucleSecOuv, &iMoteurArret, &iArretUrgence, &iEtatCaptVib);

			if(	(iPorte1Ouverte == iUSB6008_ETAT_PORTE_1_OUVERTE) 			||
					(iPorte2Ouverte == iUSB6008_ETAT_PORTE_2_OUVERTE) 			||
					(iCapotOuvert 	== iUSB6008_ETAT_CAPOT_SECURITE_OUVERT) 	||
					(iMoteurArret 	!= iUSB6008_ETAT_MOTEUR_ARRETE) 			||
					(GetGdPression() > ConvLimPression(dMAX_PRESS_START_TEST, GetGiUnitPression())) 					||
					(GetGiErrVaria() 	!= 0)									||
					(GetGiErrUsb6008() 	!= 0)									||
					(iArretUrgence 	== iUSB6008_ETAT_ARRET_URGENCE_ACTIF)		||
					(iEtatCaptVib 	==iUSB6008_ETAT_VIBRATIONS_DETECT_ACTIF))
			{
				// On cache le message d'attente d'appui sur le bouton de la boucle de sécurité
				SetCtrlAttribute (GiPanel, PANEL_MODE_MessageIndicateur, ATTR_VISIBLE, 0);
				bAffichMsgAttente = 1;
				SetGiEtat(iETAT_ATTENTE);
			}
			else
			{
				if(bAffichMsgAttBoucle)
				{
					// Demande d'appui sur le bouton de la boucle de sécurité
					SetGiErrUsb6008(Usb6008CommandeVentilationMoteur (iUSB6008_CMDE_VENTILATION_MOTEUR_INACTIFS));

					ModesAddMessageZM(sMSG_PUSH_SEC_LOOP_BUTT);
					ResetTextBox (GiPanel, PANEL_MODE_MessageIndicateur, sMSG_PUSH_SEC_LOOP_BUTT);

					// On affiche le message d'attente d'appui sur le bouton de la boucle de sécurité
					SetCtrlAttribute (GiPanel, PANEL_MODE_MessageIndicateur, ATTR_VISIBLE, 1);
					SetCtrlAttribute (GiPanel, PANEL_MODE_MessageIndicateur, ATTR_ZPLANE_POSITION, 0);

					bAffichMsgAttBoucle = 0;
				}

				// Si la gâche est ouverte
				if (GetGbGacheActive() == iGACHE_OUVERTE)
				{
					// La gache doit être fermée pour lancer l'essai
					SetGiEtat(iETAT_DEM_CLOSE_ELECTRIC_LOCK);
					bAffichMsgAttElectLock = 1;
				}

				// Si la boucle de sécurité est fermée
				if (iBoucleSecOuv != iUSB6008_ETAT_BOUCLE_SECURITE_OUVERTE)
				{
					bAffichMsgAttBoucle   = 1;
					bAffichMsgAttDepEssai = 1;
					// On cache le message d'attente d'appui sur le bouton de la boucle de sécurité
					SetCtrlAttribute (GiPanel, PANEL_MODE_MessageIndicateur, ATTR_VISIBLE, 0);
					SetGiEtat(iETAT_ATT_DEP_ESSAI);
				}
			}
		}

		if(GetGiEtat() == iETAT_ATT_DEP_ESSAI)
		{
			// Autorisation de l'ouverture gâche si le moteur ne tourne pas et si la pression est assez basse
			fVitesse = GetGfVitesse();
			if ((fabs(fVitesse) < dVIT_ABS_START_TEST)&& (GetGdPression() < ConvLimPression(dMAX_PRESS_START_TEST, GetGiUnitPression())))
				SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_OUV_GACHE, ATTR_DIMMED, 0);
			else
				SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_OUV_GACHE, ATTR_DIMMED, 1);

			RetourEtatAttente(&iPorte1Ouverte, &iPorte2Ouverte, &iCapotOuvert,
							  &iBoucleSecOuv, &iMoteurArret, &iArretUrgence, &iEtatCaptVib);

			if(	(iPorte1Ouverte == iUSB6008_ETAT_PORTE_1_OUVERTE) 			||
					(iPorte2Ouverte == iUSB6008_ETAT_PORTE_2_OUVERTE) 			||
					(iCapotOuvert 	== iUSB6008_ETAT_CAPOT_SECURITE_OUVERT) 	||
					((iMoteurArret 	!= iUSB6008_ETAT_MOTEUR_ARRETE) && (!GetGiStartCycleVitesse())) 			||
					(iBoucleSecOuv  == iUSB6008_ETAT_BOUCLE_SECURITE_OUVERTE)	||
					((GetGdPression() > ConvLimPression(dMAX_PRESS_START_TEST, GetGiUnitPression())) && (!GetGiStartCyclePression())) 						||
					(GetGiErrVaria() 	!= 0)										||
					(GetGiErrUsb6008() 	!= 0)										||
					(iArretUrgence 	== iUSB6008_ETAT_ARRET_URGENCE_ACTIF)		||
					(iEtatCaptVib 	==iUSB6008_ETAT_VIBRATIONS_DETECT_ACTIF))
			{

				bAffichMsgAttente = 1;
				SetGiEtat(iETAT_ATTENTE);
			}
			else
			{
				if(bAffichMsgAttDepEssai)
				{
					SetGiErrUsb6008(Usb6008CommandeVentilationMoteur (iUSB6008_CMDE_VENTILATION_MOTEUR_INACTIFS));
					// Mise à jour du voyant d'autorisation de départ Essai
					SetCtrlVal (GiPanel, PANEL_MODE_START_ENABLE, 1);
							SetCtrlAttribute (GiPanel,PANEL_MODE_MODE_EXE, 		ATTR_DIMMED, 0);   
					SetCtrlVal (GiPanel, PANEL_MODE_START_ENABLE2, 0x0006B025); 
					

					// Autorisation de l'appui sur les boutons de départ essai
					SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VITESSE, ATTR_DIMMED, 	0);
					SetCtrlAttribute (GiPanel, PANEL_MODE_BUT_START_PRESSION, ATTR_DIMMED, 	0);
					SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_START_VIT_PRESS, ATTR_DIMMED, 0);

					//SetCtrlAttribute (GiPanel, PANEL_MODE_BUT_START_RUN, ATTR_DIMMED, 0);

					// Demande d'appui sur le bouton de la boucle de sécurité
					ModesAddMessageZM(sMSG_OK_SECUR_LOOP_CLOSED);
					ModesAddMessageZM(sMSG_WAIT_START_TEST);

					bAffichMsgAttDepEssai = 0;
				}

				if((GetGiStartCycleVitesse()== 1)||(GetGiStartCyclePression() == 1)||(GetGiStartVitPress() == 1))
				{
					// Interdiction de l'ouverture gâche
					SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_OUV_GACHE, ATTR_DIMMED, 1);
					bAffichMsgEssEnCours = 1;
					SetGiEtat(iETAT_ESSAI_EN_COURS);
					iNbPannesSucc = 0;
				}
			}
		}

		if(GetGiEtat() ==  iETAT_ESSAI_EN_COURS)
		{
			RetourEtatAttente(&iPorte1Ouverte, &iPorte2Ouverte, &iCapotOuvert,
							  &iBoucleSecOuv, &iMoteurArret, &iArretUrgence, &iEtatCaptVib);

			if(	(iPorte1Ouverte == iUSB6008_ETAT_PORTE_1_OUVERTE) 			||
					(iPorte2Ouverte == iUSB6008_ETAT_PORTE_2_OUVERTE) 			||
					(iCapotOuvert 	== iUSB6008_ETAT_CAPOT_SECURITE_OUVERT) 	||
					(iBoucleSecOuv  == iUSB6008_ETAT_BOUCLE_SECURITE_OUVERTE)	||
					(GetGiErrVaria() 	!= 0)										||
					(GetGiErrUsb6008() 	!= 0)										||
					(iArretUrgence 	== iUSB6008_ETAT_ARRET_URGENCE_ACTIF)		||
					(iEtatCaptVib 	==iUSB6008_ETAT_VIBRATIONS_DETECT_ACTIF))
			{

				if((GetGiErrVaria() != 0) || (GetGiErrUsb6008() != 0))
					iNbPannesSucc++;
				else
					iNbPannesSucc += 2;
			}
			else
				iNbPannesSucc = 0;

			if(iNbPannesSucc >= 4)
			{
				bAffichMsgAttente = 1;
				SetGiEtat(iETAT_ATTENTE);

				if (iPorte1Ouverte == iUSB6008_ETAT_PORTE_1_OUVERTE)
					sprintf(sMsg, "%s: Door 1 Opened", sMSG_ERR_SECU_DFLT_DETEC);
				else if (iPorte2Ouverte == iUSB6008_ETAT_PORTE_2_OUVERTE)
					sprintf(sMsg, "%s: Door 2 Opened", sMSG_ERR_SECU_DFLT_DETEC);
				else if (iCapotOuvert 	== iUSB6008_ETAT_CAPOT_SECURITE_OUVERT)
					sprintf(sMsg, "%s: Security Hood Opened", sMSG_ERR_SECU_DFLT_DETEC);
				else if (iEtatCaptVib 	== iUSB6008_ETAT_VIBRATIONS_DETECT_ACTIF)
					sprintf(sMsg, "%s: ********** WARNING: VIBRATIONS DETECTED !!! **********", sMSG_ERR_SECU_DFLT_DETEC);
				else if (iBoucleSecOuv  == iUSB6008_ETAT_BOUCLE_SECURITE_OUVERTE)
					sprintf(sMsg, "%s: Security Loop Opened", sMSG_ERR_SECU_DFLT_DETEC);
				else if (GetGiErrVaria() 	!= 0)
					sprintf(sMsg, "%s: Variator communication error", sMSG_ERR_SECU_DFLT_DETEC);
				else if (GetGiErrUsb6008() 	!= 0)
					sprintf(sMsg, "%s: USB6008 communication error", sMSG_ERR_SECU_DFLT_DETEC);
				else if (iArretUrgence 	== iUSB6008_ETAT_ARRET_URGENCE_ACTIF)
					sprintf(sMsg, "%s: Emergency stop pushed", sMSG_ERR_SECU_DFLT_DETEC);
				else
					strcpy(sMsg, sMSG_ERR_SECU_DFLT_DETEC);

				AffStatusPanel(GiPanel, GiStatusPanel, iLEVEL_WARNING,"Cycle Error", sMsg, GetPointerToGiPanelStatusDisplayed(), &iAffMsg);
				ReleasePointerToGiPanelStatusDisplayed();

				SetGiAffMsg(iAffMsg);
			}
			else
			{
				if(bAffichMsgEssEnCours)
				{
					SetGiErrUsb6008(Usb6008CommandeOuvertureGache (iUSB6008_CMDE_OUVERTURE_GACHE_INACTIVE));
					if(GetGiErrUsb6008() == 0)
					{
						SetGbGacheActive(iGACHE_FERMEE);
						SetCtrlAttribute (GiPanel, PANEL_MODE_BUTT_OUV_GACHE, ATTR_LABEL_TEXT, "Open Electric Lock");
					}

					// Mise à jour du voyant d'autorisation de départ Essai
					SetCtrlVal (GiPanel, PANEL_MODE_START_ENABLE, 1);
					SetCtrlAttribute (GiPanel,PANEL_MODE_MODE_EXE, 		ATTR_DIMMED, 0);   
					SetCtrlVal (GiPanel, PANEL_MODE_START_ENABLE2, 0x0006B025); 
					
					// Demande d'appui sur le bouton de la boucle de sécurité
					ModesAddMessageZM(sMSG_TEST_RUNNING);

					bAffichMsgEssEnCours = 0;
				}

				// Si tous les cycles sont terminés, alors on revient à l'accueil
				if((GetGiStartCycleVitesse()== 0) && (GetGiStartCyclePression() == 0) && (GetGiStartVitPress() == 0))
				{
					if ((GetGbEndCycleVitesse () == 1) && (GetGbEndCyclePression() == 1))
					{
						bAffichMsgAttente = 1;
						SetGiEtat(iETAT_ATTENTE);
					}
				}
			}
		}

		ProcessSystemEvents();
		dTime = Timer();
		SyncWait(dTime, dDELAY_SECURITY_PROCESS);
	}

	return(0);
}

// DEBUT ALGO
//***************************************************************************
//int CVICALLBACK OuvertureGache (int panel, int control, int event,
//		void *callbackData, int eventData1, int eventData2)
//***************************************************************************
//  - Paramètres CVI
//
//  - Commande l'ouverture de la gache
//
//  - 1 si problème détecté, 0 sinon
//***************************************************************************
// FIN ALGO
int CVICALLBACK OuvertureGache (int panel, int control, int event,
								void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{
		case EVENT_COMMIT:
			// On ne peut commander la gâche que si un essai est en cours
			if (GetGiEtat() != iETAT_ESSAI_EN_COURS)
			{
				if(GetGbGacheActive() == iGACHE_FERMEE)
				{
					// Commande de la gâche
					SetGiErrUsb6008(Usb6008CommandeOuvertureGache (iUSB6008_CMDE_OUVERTURE_GACHE_ACTIVE));
					// S'il n'y a pas d'erreur
					if(GetGiErrUsb6008() == 0)
					{
						SetCtrlAttribute (GiPanel,PANEL_MODE_MODE_EXE, 		ATTR_DIMMED, 1); 
						SetGbGacheActive(iGACHE_OUVERTE);
						SetCtrlAttribute (panel, control, ATTR_LABEL_TEXT, "Close Electric Lock");
					}
					else
					{
					SetCtrlAttribute (GiPanel,PANEL_MODE_MODE_EXE, 		ATTR_DIMMED, 0); 	
					}
				}
				else
				{
					// Commande de la gâche
					SetGiErrUsb6008(Usb6008CommandeOuvertureGache (iUSB6008_CMDE_OUVERTURE_GACHE_INACTIVE));

					// S'il n'y ap pas d'erreur
					if(GetGiErrUsb6008() == 0)
					{
						SetCtrlAttribute (GiPanel,PANEL_MODE_MODE_EXE, 		ATTR_DIMMED, 1); 
						SetGbGacheActive(iGACHE_FERMEE);
						SetCtrlAttribute (panel, control, ATTR_LABEL_TEXT, "Open Electric Lock");
					}
					else 
					{
						SetCtrlAttribute (GiPanel,PANEL_MODE_MODE_EXE, 		ATTR_DIMMED, 0); 	
					}
				}
			}
			break;
	}
	return 0;
}

// DEBUT ALGO
//***************************************************************************
// int CVICALLBACK DetailStatus (int panel, int control, int event,
//		void *callbackData, int eventData1, int eventData2)
//***************************************************************************
//  - Paramètres CVI
//
//  - Affiche le détail des status
//
//  - 1 si problème détecté, 0 sinon
//***************************************************************************
// FIN ALGO
int CVICALLBACK DetailStatus (int panel, int control, int event,
							  void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{
		case EVENT_COMMIT:
			if(GetGiPanelStatusDisplayed() == 0)
			{
				// On cache l'écran principal
				//SetPanelAttribute (GiPanel, ATTR_VISIBLE, 0);
				DisplayPanel(GiStatusPanel);

				// Modification du 07/07/2011 par CB ------ DEBUT --------------
				SetCtrlAttribute (GiStatusPanel, PANEL_STAT_MESSAGES, ATTR_TEXT_BOLD, 0);
				SetCtrlAttribute (GiStatusPanel, PANEL_STAT_MESSAGES, ATTR_TEXT_BGCOLOR, VAL_WHITE);
				SetCtrlAttribute (GiStatusPanel, PANEL_STAT_MESSAGES, ATTR_TEXT_COLOR,  VAL_BLACK);
				// Modification du 07/07/2011 par CB ------ FIN --------------

				//Effacement de la zone de message de l'écran de status
				ResetTextBox (GiStatusPanel, PANEL_STAT_MESSAGES, "");
				SetGiPanelStatusDisplayed(1);
				SetGiAffMsg(0);
			}
			break;
	}

	return 0;
}

// DEBUT ALGO
//***************************************************************************
//int CVICALLBACK CloseStatusPanel (int panel, int control, int event,
//		void *callbackData, int eventData1, int eventData2)
//***************************************************************************
//  - Paramètres CVI
//
//  - Ferme l'écran de détail des status
//
//  - 1 si problème détecté, 0 sinon
//***************************************************************************
// FIN ALGO
int CVICALLBACK CloseStatusPanel (int panel, int control, int event,
								  void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{
		case EVENT_COMMIT:
			//Effacement de la zone de message de l'écran de status
			ResetTextBox (GiStatusPanel, PANEL_STAT_MESSAGES, "");
			// On cache l'écran d'affichage du status
			HidePanel (GiStatusPanel);
			//On refait passer à zéro l'indicateur de message affiché
			SetGiAffMsg(0);
			// On affiche l'écran principal
			//SetPanelAttribute (GiPanel, ATTR_VISIBLE, 1);
			SetGiPanelStatusDisplayed(0);
			break;
	}

	return 0;
}

int CVICALLBACK AffResult (int panel, int control, int event,
						   void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{
		case EVENT_COMMIT:

			break;
	}
	return 0;
}

///// Choix du comment : "pre_condition", "script" ou "post_condition"
/*
int CVICALLBACK box_pre (int panel, int control, int event,
						 void *callbackData, int eventData1, int eventData2)
{
	int valueZero = 0;

	switch (event)
	{
		case EVENT_COMMIT:



			   SetCtrlVal(panel,PANEL_ADD_BOX_SCRIPT,valueZero);
			   SetCtrlVal(panel,PANEL_ADD_BOX_POST,valueZero);


			break;
	}
	return 0;
}

int CVICALLBACK box_script (int panel, int control, int event,
							void *callbackData, int eventData1, int eventData2)
{
	int valueZero = 0;

	switch (event)
	{
		case EVENT_COMMIT:

			  SetCtrlVal(panel,PANEL_ADD_BOX_PRE,valueZero);
			  SetCtrlVal(panel,PANEL_ADD_BOX_POST,valueZero);


			break;
	}
	return 0;
}

int CVICALLBACK box_post (int panel, int control, int event,
						  void *callbackData, int eventData1, int eventData2)
{
	int valueZero = 0;

	switch (event)
	{
		case EVENT_COMMIT:

			  SetCtrlVal(panel,PANEL_ADD_BOX_SCRIPT,valueZero);
			  SetCtrlVal(panel,PANEL_ADD_BOX_PRE,valueZero);


			break;
	}
	return 0;
}
*/

//***********************************************************************************************
// Modif - MaximePAGES 10/06/2020
// Choix du comment : "pre_condition", "script" ou "post_condition" with new buttons (radiobutton)
//***********************************************************************************************

int CVICALLBACK box_pre (int panel, int control, int event,
						 void *callbackData, int eventData1, int eventData2)
{
	int valueZero = 0;
	int valueOne = 1;

	switch (event)
	{
		case EVENT_COMMIT:

			SetCtrlVal(panel,PANEL_ADD_BOX_PRE,valueOne);
			SetCtrlVal(panel,PANEL_ADD_BOX_SCRIPT,valueZero);
			SetCtrlVal(panel,PANEL_ADD_BOX_POST,valueZero);

			SetCtrlVal(GiPopupAdd2, PANEL_ADD_LEDdebug, 0); //debug MaximePAGES - 10/06/2020


			break;
	}
	return 0;
}

int CVICALLBACK box_script (int panel, int control, int event,
							void *callbackData, int eventData1, int eventData2)
{
	int valueZero = 0;
	int valueOne = 1;

	switch (event)
	{
		case EVENT_COMMIT:

			SetCtrlVal(panel,PANEL_ADD_BOX_SCRIPT,valueOne);
			SetCtrlVal(panel,PANEL_ADD_BOX_PRE,valueZero);
			SetCtrlVal(panel,PANEL_ADD_BOX_POST,valueZero);

			SetCtrlVal(GiPopupAdd2, PANEL_ADD_LEDdebug, 0); //debug MaximePAGES - 10/06/2020


			break;
	}
	return 0;
}

int CVICALLBACK box_post (int panel, int control, int event,
						  void *callbackData, int eventData1, int eventData2)
{
	int valueZero = 0;
	int valueOne = 1;

	switch (event)
	{
		case EVENT_COMMIT:

			SetCtrlVal(panel,PANEL_ADD_BOX_POST,valueOne);
			SetCtrlVal(panel,PANEL_ADD_BOX_SCRIPT,valueZero);
			SetCtrlVal(panel,PANEL_ADD_BOX_PRE,valueZero);


			break;
	}
	return 0;
}











//***********************************************************************************************
// Modif - Carolina 02/2019
// This button it is not being used
//***********************************************************************************************
int CVICALLBACK insert_step (int panel, int control, int event,
							 void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{
		case EVENT_COMMIT:

			break;
		case EVENT_RIGHT_CLICK:

			break;
	}
	return 0;
}
//************MARVYN 12/07/2021*************
// fonction to load an initial configuration
//******************************************
int CVICALLBACK InitConfig (int panel, int control, int event,
							 void *callbackData, int eventData1, int eventData2)
{

	int curseur_chaine=0;
	int compteur_chaine=0;
	int fin_chaine=0;
	char config_name[MAX_PATHNAME_LEN]="";
	char *anumConf;

	
	int curseur_chaine2=0;
	int compteur_chaine2=0;
	int fin_chaine2=0;
	char dirProject_name[MAX_PATHNAME_LEN]="";
	char defaultDir[MAX_PATHNAME_LEN] = "D:\\";
	char sProjectDir[MAX_PATHNAME_LEN];
	int errDir=0;
	int oldValue;
	char *LD;
	
	char database_name[MAX_PATHNAME_LEN]="";
	char sIFandIBthreshold[10]="";
	char *DB;
	int end = 1;
	char range[30];
	char parameterName[100];
	char WUIDname[100];
	char WUIDvalue[100];
	char range1[30];
	char range2[30];
	char range3[30];
	int cont = 0;
	
	
	errDir = GetProjectDir(sProjectDir);
	if(errDir == -1 || errDir == -2)
		strcpy(sProjectDir, defaultDir);
	
	// Formation du chemin vers le répertoire des configurations
	//GetProjectDir(sProjectDir);
	
	
	switch (event)
	{
		case EVENT_COMMIT:
			
		
			
			indexInterfaceStruct = 0;
			if (strcmp(configInit('A'),"\n") !=0 && strcmp(configInit('A'),"\0") !=0 )
			{
				
			curseur_chaine=0;
			compteur_chaine=0;
			fin_chaine=0;
			nbParameter = 0;
			strcpy(config_name,"");	
				
		
			ProgressBar_ConvertFromSlide(GiPanel,PANEL_MODE_PROGRESSBAR); //Initialization of the progress bar
			ProgressBar_ConvertFromSlide(GiPanel,PANEL_MODE_PROGRESSBARSEQ); //Initialization of the seq progress bar
		
			
			SetCtrlVal (panel, PANEL_MODE_LED1, 1);
			SetCtrlVal (panel, PANEL_MODE_LED12, 0x0006B025);
			SetCtrlVal (panel, PANEL_MODE_INDICATOR_CONFIG_ANUM,configName(configInit('A')));
			anumConf=configInit('A');
			anumConf[strlen(anumConf)-1]=NULL;
			strcpy(GsAnumCfgFile, anumConf);
			free(chaine);
			iAnumLoad=1;   // Condition pour le boutton Add script 
			}
			
			if (strcmp(configInit('L'),"\0") !=0 && strcmp(configInit('L'),"\n") !=0)
			{
				
			curseur_chaine2=0;
			compteur_chaine2=0;
			fin_chaine2=0;
			strcpy(dirProject_name,"");
			errDir=0;   	
			
			GetProjectDir(GsResultsDirPath);
			strcat(GsResultsDirPath, "\\"sREP_RESULTATS);
			oldValue = SetBreakOnLibraryErrors (0);
			MakeDir(GsResultsDirPath);  
			SetBreakOnLibraryErrors (oldValue);
			
		
			SetCtrlVal (panel, PANEL_MODE_LED3, 1);
			SetCtrlVal (panel, PANEL_MODE_LED32, 0x0006B025); 
			SetCtrlVal (panel, PANEL_MODE_INDICATOR_SET_LOG_DIR,configName(configInit('L')));
			
			LD=configInit('L');
			//LD[strlen(LD)-1]=NULL;
			//LD[strlen(LD)]='\0';
			strcpy(GsLogPath, LD);
			
			//strcpy(GsLogPath, "D:\\MarvynPANNETIER\\Log_Dir");
			
			strcpy(GsLogPathInit,GsLogPath);
			//free(chaine);
			//printf("marvyn change : %s  \n",GsLogPath);
			iSetLogDirLoad = 1;
			}
			
			if (strcmp(configInit('D'),"\n") !=0 && strcmp(configInit('D'),"\0") !=0)
			{
				
			curseur_chaine2=0;
			compteur_chaine2=0;
			fin_chaine2=0;
		    strcpy(database_name,"");
			strcpy(sIFandIBthreshold,"");	
			nbParameter=0;
			
			iDataBaseLoad=1;   // Condition pour le boutton Add script 
			
			SetWaitCursor (1);
				//display of the archive database - a green led inform that an archive was loaded
				SetCtrlVal (panel, PANEL_MODE_LED2, 1);
				SetCtrlVal (panel, PANEL_MODE_LED22, 0x0006B025); 
				SetCtrlVal (panel, PANEL_MODE_INDICATOR_DATABASE,configName(configInit('D')));
				ExcelRpt_ApplicationNew(VFALSE,&applicationHandleProject);
				DB=configInit('D');
				DB[strlen(DB)-1]=NULL;
				//DB = "D:\\MarvynPANNETIER\\SetUp A172\\Copie de 2Database_A172_SW2130.xlsx";
				//printf("DB = %s\n",DB);
				strcpy(GsCheckCfgFile, DB); 
				free(chaine); 

				//MODIF MaximePAGES 03/07/2020 - E003 ***********************************
				ExcelRpt_WorkbookOpen (applicationHandleProject, GsCheckCfgFile, &workbookHandledata);
				ExcelRpt_GetWorksheetFromIndex(workbookHandledata, 1, &worksheetHandledata1);
				ExcelRpt_GetWorksheetFromIndex(workbookHandledata, 2, &worksheetHandledata2);
				ExcelRpt_GetWorksheetFromIndex(workbookHandledata, 2, &worksheetHandledata);

			
				GetALLFramesTypes();
				
			/*	for (int j=0;j<4;j++)
				{
					//printf("Interfaces[%d].name = %s\n",j,Interfaces[j].name);
					//printf("Interfaces[%d].columnParam = %s\n",j,Interfaces[j].columnParam);
					//printf("Interfaces[%d].columnAttribute = %s\n",j,Interfaces[j].columnAttribute);
				}  */
			
				
				
				//Modif MaximePAGES 13/08/2020 - new IHM for WU ID
				for(int i=2; end==1; i++)
				{
					sprintf(range,"O%d",i);
					ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, WUIDname);

					if (strcmp(WUIDname, "") == 0)
					{
						end=0;  //the end was found
					}
					else
					{
						if (strcmp(WUIDname, "ID1") == 0)
						{
							sprintf(range,"P%d",i);
							ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, WUIDvalue);
							SetCtrlVal(panel,PANEL_MODE_WUIDSTAND1,WUIDvalue);
							
							//Modif MaximePAGES 14/08/2020
							strcpy(tabStandWUID[0],WUIDvalue);

						}
						
						if (strcmp(WUIDname, "IDDiag1") == 0)
						{
							sprintf(range,"P%d",i);
							ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, WUIDvalue);
							SetCtrlVal(panel,PANEL_MODE_WUIDDIAG1,WUIDvalue);
							
							//Modif MaximePAGES 26/08/2020
							strcpy(tabDiagWUID[0],WUIDvalue);

						}
						
						if (strcmp(WUIDname, "IDSWIdent1") == 0)
						{
							sprintf(range,"P%d",i);
							ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, WUIDvalue);
							//SetCtrlVal(panel,PANEL_MODE_WUIDDIAG1,WUIDvalue);
							
							//Modif MaximePAGES 26/08/2020
							strcpy(tabSWIdentWUID[0],WUIDvalue);

						}
						
						
						if (strcmp(WUIDname, "ID2") == 0)
						{
							sprintf(range,"P%d",i);
							ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, WUIDvalue);
							SetCtrlVal(panel,PANEL_MODE_WUIDSTAND2,WUIDvalue);
							
							//Modif MaximePAGES 14/08/2020
							strcpy(tabStandWUID[1],WUIDvalue);
							
						}
						
						if (strcmp(WUIDname, "IDDiag2") == 0)
						{
							sprintf(range,"P%d",i);
							ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, WUIDvalue);
							SetCtrlVal(panel,PANEL_MODE_WUIDDIAG2,WUIDvalue);
							
							//Modif MaximePAGES 26/08/2020
							strcpy(tabDiagWUID[1],WUIDvalue);
							
						}
						
						if (strcmp(WUIDname, "IDSWIdent2") == 0)
						{
							sprintf(range,"P%d",i);
							ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, WUIDvalue);
						//	SetCtrlVal(panel,PANEL_MODE_WUIDDIAG1,WUIDvalue);
							
							//Modif MaximePAGES 26/08/2020
							strcpy(tabSWIdentWUID[1],WUIDvalue);

						}
						
						
					}
				}
				

				end =1;

				//Get IF and IB threshold value from DataBase
				ExcelRpt_GetCellValue (worksheetHandledata1, "N2", CAVT_CSTRING, sIFandIBthreshold);
				IFandIBthreshold = atoi(sIFandIBthreshold);

				
				//Modif MaximePAGES 31/07/2020 - generate a tab of parameters from Database
				//we save all parameters (name, value, unit) inside a tab
				for(int row=2; end==1; row++)
				{
					sprintf(range,"A%d",row);
					ExcelRpt_GetCellValue (worksheetHandledata2, range, CAVT_CSTRING, parameterName);

					if (strcmp(parameterName, "") == 0)
					{
						end=0;  //the end was found
					}
					else
					{
						if (strcmp(parameterName, "Continuous") == 0)  
						{
							cont = 1;
						}
						nbParameter++;
					}
					
				}
			
				if ( cont == 0)
				{
					//printf("row=%d\n",row-1);
					sprintf(range1,"A%d",row-1); 
					ExcelRpt_SetCellValue (worksheetHandledata2,range1 ,CAVT_CSTRING,"Continuous");
					sprintf(range2,"B%d",row-1); 
					ExcelRpt_SetCellValue (worksheetHandledata2,range2 ,CAVT_CSTRING,"0"); 
					sprintf(range3,"C%d",row-1); 
					ExcelRpt_SetCellValue (worksheetHandledata2,range3 ,CAVT_CSTRING,"Frames"); 
					ExcelRpt_WorkbookSave (workbookHandledata,NULL,NULL);
					nbParameter++;
				}  
				myParameterTab = malloc (nbParameter * sizeof (struct Parameter));

				generateParameterTab (worksheetHandledata2) ;

				SetCtrlAttribute (GiPanel, PANEL_MODE_TABLE_SCRIPT, ATTR_LABEL_COLOR , 	VAL_LT_GRAY);


				//***********************************************************************

				//MODIF MaximePAGES 23/06/2020 - E002 ***********************************
				databasePID = returnLastExcelPID();  // we save the DataBase Excel PID for later
				myListProcExcelPID = initialisationList(); // we initialize the list of PID excel
				//***********************************************************************
				SetWaitCursor (0);
			}
			
			break;
			}
			return 0;
}
	
	
	
	
	
	
	
	
	
	
	
//***********************************************************************************************
// Modif - Carolina 02/2019
// Test with button select Anum config - for ergonomic
//***********************************************************************************************
int CVICALLBACK button_anum (int panel, int control, int event,
							 void *callbackData, int eventData1, int eventData2)
{

	char sProjectDir[MAX_PATHNAME_LEN];
	int iRet=0;
	int curseur_chaine=0;
	int compteur_chaine=0;
	int fin_chaine=0;
	char config_name[MAX_PATHNAME_LEN]="";
	char *anumConf;
	
	// Formation du chemin vers le répertoire des configurations
	GetProjectDir(sProjectDir);
	
	
	switch (event)
	{
		case EVENT_COMMIT:

//MODIF MaximePAGES 1/07/2020 - Script Progres Bar E402 *******

			ProgressBar_ConvertFromSlide(GiPanel,PANEL_MODE_PROGRESSBAR); //Initialization of the progress bar
			ProgressBar_ConvertFromSlide(GiPanel,PANEL_MODE_PROGRESSBARSEQ); //Initialization of the seq progress bar
			//SetCtrlAttribute (GiPanel, PANEL_MODE_PROGRESSBAR, ATTR_DISABLE_PANEL_THEME, 1);
			//SetCtrlAttribute (GiPanel, PANEL_MODE_PROGRESSBARSEQ, ATTR_DISABLE_PANEL_THEME, 1);
			//ProgressBar_SetAttribute (GiPanel,PANEL_MODE_PROGRESSBARSEQ , ATTR_PROGRESSBAR_BAR_COLOR,0x003399L );

		
			//iAnumLoad=0;
			//iRet = FileSelectPopup (sProjectDir, "*."".cfg", "", "", VAL_SELECT_BUTTON, 0, 1, 1, 0, GsAnumCfgFile);
			iRet = FileSelectPopupEx (sProjectDir, "*.cfg", "", "", VAL_SELECT_BUTTON, 0, 1, GsAnumCfgFile);

			
			if(iRet > 0)
			{
				iAnumLoad=1;   // Condition pour le boutton Add script

				//Récupération du nom de la config ANum
				for(curseur_chaine=strlen(GsAnumCfgFile); fin_chaine==0 ; curseur_chaine--)
				{
					if( GsAnumCfgFile[curseur_chaine]=='\\' ) // search the separator folder\folder2\name
					{
						fin_chaine=1;
						for( compteur_chaine=0; (curseur_chaine+compteur_chaine) < strlen(GsAnumCfgFile) ; compteur_chaine++)
						{
							config_name[compteur_chaine]=GsAnumCfgFile[curseur_chaine+compteur_chaine+1];
						}
					}
				}
				//display of the archive Anum config - a green led inform that an archive was loaded
				SetCtrlVal (panel, PANEL_MODE_LED1, 1);
				SetCtrlVal (panel, PANEL_MODE_LED12, 0x0006B025);
				SetCtrlVal (panel, PANEL_MODE_INDICATOR_CONFIG_ANUM, config_name);

			}
			else  //0 VAL_NO_FILE_SELECTED
			{
				iAnumLoad=0;
				strcpy(config_name, "");
				strcpy(GsAnumCfgFile, "");
				SetCtrlVal(panel,PANEL_MODE_INDICATOR_CONFIG_ANUM,"");
				SetCtrlVal (panel, PANEL_MODE_LED1, 0);
				SetCtrlVal (panel, PANEL_MODE_LED12, 0x00000000);
				return 0;
			}

			break;
				
	}
	return 0;
}


//***********************************************************************************************
// Modif - Carolina 02/2019
// Test with button select Data base - for ergonomic
//***********************************************************************************************
int CVICALLBACK  button_database (int panel, int control, int event,
								 void *callbackData, int eventData1, int eventData2)
{
	char sProjectDir[MAX_PATHNAME_LEN];
	int iRet;
	int curseur_chaine2=0;
	int compteur_chaine2=0;
	int fin_chaine2=0;
	char database_name[MAX_PATHNAME_LEN]="";
	char sIFandIBthreshold[10]="";
	char *DB="";
	int end = 1;
	char range[30];
	char range1[30];
	char range2[30];
	char range3[30];
	char parameterName[100];
	char WUIDname[100];
	char WUIDvalue[100];  
	char name[150];
	int cont = 0;
	
	// Formation du chemin vers le répertoire des configurations
	GetProjectDir(sProjectDir);
	
	

	switch (event)
	{
		case EVENT_COMMIT:

			indexInterfaceStruct = 0;
		
			nbParameter = 0;
			//free(myParameterTab);

			iRet = FileSelectPopupEx (sProjectDir, "*.xlsx", "", "", VAL_SELECT_BUTTON, 0, 1, GsCheckCfgFile);
			//printf("GsCheckCfgFile = %s\n",GsCheckCfgFile);
			
			//iRet = FileSelectPopup (sProjectDir, "*."".xlsx", "", "", VAL_SELECT_BUTTON, 0, 1, 1, 0, GsCheckCfgFile);
			if(iRet > 0)
			{
				iDataBaseLoad=1;   // Condition pour le boutton Add script

				//Récupération du nom de la database
				for(curseur_chaine2=strlen(GsCheckCfgFile); fin_chaine2==0 ; curseur_chaine2--)
				{
					if( GsCheckCfgFile[curseur_chaine2]=='\\' )
					{
						fin_chaine2=1;
						for( compteur_chaine2=0; (curseur_chaine2+compteur_chaine2) < strlen(GsCheckCfgFile) ; compteur_chaine2++)
						{
							database_name[compteur_chaine2]=GsCheckCfgFile[curseur_chaine2+compteur_chaine2+1];
						}
					}
				}

			   	SetWaitCursor (1);
				//display of the archive database - a green led inform that an archive was loaded
				SetCtrlVal (panel, PANEL_MODE_LED2, 1);
				SetCtrlVal (panel, PANEL_MODE_LED22, 0x0006B025); 
				SetCtrlVal (panel, PANEL_MODE_INDICATOR_DATABASE, database_name);
				ExcelRpt_ApplicationNew(VFALSE,&applicationHandleProject);


				//MODIF MaximePAGES 03/07/2020 - E003 ***********************************
				ExcelRpt_WorkbookOpen (applicationHandleProject, GsCheckCfgFile, &workbookHandledata);
				ExcelRpt_GetWorksheetFromIndex(workbookHandledata, 1, &worksheetHandledata1);
				ExcelRpt_GetWorksheetFromIndex(workbookHandledata, 2, &worksheetHandledata2);
				ExcelRpt_GetWorksheetFromIndex(workbookHandledata, 2, &worksheetHandledata);

				
				//Modif MaximePAGES 13/08/2020 - new IHM for WU ID
				for(int i=2; end==1; i++)
				{
					sprintf(range,"O%d",i);
					ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, WUIDname);

					if (strcmp(WUIDname, "") == 0)
					{
						end=0;  //the end was found
					}
					else
					{
						if (strcmp(WUIDname, "ID1") == 0)
						{
							sprintf(range,"P%d",i);
							ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, WUIDvalue);
							SetCtrlVal(panel,PANEL_MODE_WUIDSTAND1,WUIDvalue);
							
							//Modif MaximePAGES 14/08/2020
							strcpy(tabStandWUID[0],WUIDvalue);

						}
						
						if (strcmp(WUIDname, "IDDiag1") == 0)
						{
							sprintf(range,"P%d",i);
							ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, WUIDvalue);
							SetCtrlVal(panel,PANEL_MODE_WUIDDIAG1,WUIDvalue);
							
							//Modif MaximePAGES 26/08/2020
							strcpy(tabDiagWUID[0],WUIDvalue);

						}
						
						if (strcmp(WUIDname, "IDSWIdent1") == 0)
						{
							sprintf(range,"P%d",i);
							ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, WUIDvalue);
							//SetCtrlVal(panel,PANEL_MODE_WUIDDIAG1,WUIDvalue);
							
							//Modif MaximePAGES 26/08/2020
							strcpy(tabSWIdentWUID[0],WUIDvalue);

						}
						
						
						if (strcmp(WUIDname, "ID2") == 0)
						{
							sprintf(range,"P%d",i);
							ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, WUIDvalue);
							SetCtrlVal(panel,PANEL_MODE_WUIDSTAND2,WUIDvalue);
							
							//Modif MaximePAGES 14/08/2020
							strcpy(tabStandWUID[1],WUIDvalue);
							
						}
						
						if (strcmp(WUIDname, "IDDiag2") == 0)
						{
							sprintf(range,"P%d",i);
							ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, WUIDvalue);
							SetCtrlVal(panel,PANEL_MODE_WUIDDIAG2,WUIDvalue);
							
							//Modif MaximePAGES 26/08/2020
							strcpy(tabDiagWUID[1],WUIDvalue);
							
						}
						
						if (strcmp(WUIDname, "IDSWIdent2") == 0)
						{
							sprintf(range,"P%d",i);
							ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, WUIDvalue);
						//	SetCtrlVal(panel,PANEL_MODE_WUIDDIAG1,WUIDvalue);
							
							//Modif MaximePAGES 26/08/2020
							strcpy(tabSWIdentWUID[1],WUIDvalue);

						}
						
						
					}
				}
				

				end =1;

				//Get IF and IB threshold value from DataBase
				ExcelRpt_GetCellValue (worksheetHandledata1, "N2", CAVT_CSTRING, sIFandIBthreshold);
				IFandIBthreshold = atoi(sIFandIBthreshold);

				
				
				//Modif MaximePAGES 31/07/2020 - generate a tab of parameters from Database
				//we save all parameters (name, value, unit) inside a tab
				for(int row=2; end==1; row++)
				{
					sprintf(range,"A%d",row);
					ExcelRpt_GetCellValue (worksheetHandledata2, range, CAVT_CSTRING, parameterName);

					if (strcmp(parameterName, "") == 0)
					{
						end=0;  //the end was found
					}
					else
					{
						if (strcmp(parameterName, "Continuous") == 0)  
						{
							cont = 1;
						}
						nbParameter++;
					}

				}
				
				if ( cont == 0)
				{
					//printf("row=%d\n",row-1);
					sprintf(range1,"A%d",row-1); 
					ExcelRpt_SetCellValue (worksheetHandledata2,range1 ,CAVT_CSTRING,"Continuous");
					sprintf(range2,"B%d",row-1); 
					ExcelRpt_SetCellValue (worksheetHandledata2,range2 ,CAVT_CSTRING,"0"); 
					sprintf(range3,"C%d",row-1); 
					ExcelRpt_SetCellValue (worksheetHandledata2,range3 ,CAVT_CSTRING,"Frames"); 
					ExcelRpt_WorkbookSave (workbookHandledata,NULL,NULL);
					nbParameter++;
				}
				cont = 0;
				myParameterTab = malloc (nbParameter * sizeof (struct Parameter));

				generateParameterTab(worksheetHandledata2) ;

				SetCtrlAttribute (GiPanel, PANEL_MODE_TABLE_SCRIPT, ATTR_LABEL_COLOR , 	VAL_LT_GRAY);


				//***********************************************************************

				//MODIF MaximePAGES 23/06/2020 - E002 ***********************************
				databasePID = returnLastExcelPID();  // we save the DataBase Excel PID for later
				//printf("database PID = %d\n",databasePID);
				myListProcExcelPID = initialisationList(); // we initialize the list of PID excel
				//***********************************************************************
				SetWaitCursor (0);
			}
			else //0 VAL_NO_FILE_SELECTED
			{
				iDataBaseLoad=0;
				strcpy(database_name, "");
				strcpy(GsCheckCfgFile, "");
				SetCtrlVal(panel,PANEL_MODE_INDICATOR_DATABASE,"");
				SetCtrlVal (panel, PANEL_MODE_LED2, 0);
				SetCtrlVal (panel, PANEL_MODE_LED22, 0x00000000);
				SetCtrlVal(panel,PANEL_MODE_WUIDSTAND1,""); 
				SetCtrlVal(panel,PANEL_MODE_WUIDSTAND2,""); 
				SetCtrlVal(panel,PANEL_MODE_WUIDDIAG1,""); 
				SetCtrlVal(panel,PANEL_MODE_WUIDDIAG2,""); 
				
				strcpy(tabStandWUID[0],"");  
				strcpy(tabStandWUID[1],"");
				strcpy(tabDiagWUID[0],"");  
				strcpy(tabDiagWUID[1],"");  
				strcpy(tabSWIdentWUID[0],"");  
				strcpy(tabSWIdentWUID[1],"");  
				
				return 0;
			}


			break;
		
			
	}

	return 0;
}
//***********************************************************************************************
// Modif - Carolina 02/2019
// Test with button Select Dir - shows the path of the log results - for ergonomic
//***********************************************************************************************
int CVICALLBACK button_set_log_dir (int panel, int control, int event,
									void *callbackData, int eventData1, int eventData2)
{
	/*char sProjectDir[MAX_PATHNAME_LEN];
	int iRet;
	// Formation du chemin vers le répertoire des configurations
	GetProjectDir(sProjectDir); */

	int curseur_chaine2=0;
	int compteur_chaine2=0;
	int fin_chaine2=0;
	char dirProject_name[MAX_PATHNAME_LEN]="";
	char defaultDir[MAX_PATHNAME_LEN] = "D:\\";
	char sProjectDir[MAX_PATHNAME_LEN];
	int iRet = 0, errDir=0;
	int oldValue;
	char *LD;
	
	errDir = GetProjectDir(sProjectDir);
	if(errDir == -1 || errDir == -2)
		strcpy(sProjectDir, defaultDir);

	switch (event)
	{
		case EVENT_COMMIT:

			//MARVYN : utile ? sauv en double ?
			//Modif MaximePAGES 21/08/2020
			GetProjectDir(GsResultsDirPath);
			strcat(GsResultsDirPath, "\\"sREP_RESULTATS);
			oldValue = SetBreakOnLibraryErrors (0);
			MakeDir(GsResultsDirPath);  
			SetBreakOnLibraryErrors (oldValue);
			
		
			iRet = 	DirSelectPopupEx(sProjectDir, "Select Log Directory", GsLogPath);
			//printf("initial code : %s  \n",GsLogPath);      
			//iRet = DirSelectPopup (sProjectDir, "Select Log Directory", 1, 1, GsLogPath);
			strcpy(GsLogPathInit,GsLogPath);
			if(iRet > 0)   //0 VAL_NO_DIRECTORY_SELECTED
			{
				iSetLogDirLoad = 1;
				for(curseur_chaine2=strlen(GsLogPath); fin_chaine2==0 ; curseur_chaine2--)
				{
					if( GsLogPath[curseur_chaine2]=='\\' )
					{
						fin_chaine2=1;
						for( compteur_chaine2=0; (curseur_chaine2+compteur_chaine2) < strlen(GsLogPath) ; compteur_chaine2++)
						{
							dirProject_name[compteur_chaine2]=GsLogPath[curseur_chaine2+compteur_chaine2+1];
						}
					}
				}

				//display of the archive database - a green led inform that an archive was loaded
				SetCtrlVal (panel, PANEL_MODE_LED3, 1);
				SetCtrlVal (panel, PANEL_MODE_LED32, 0x0006B025); 
				//SetCtrlVal (panel, PANEL_MODE_INDICATOR_SET_LOG_DIR,GsLogPath);
				SetCtrlVal (panel, PANEL_MODE_INDICATOR_SET_LOG_DIR, dirProject_name);
				//SetCtrlVal (panel, PANEL_MODE_INDICATOR_SET_LOG_DIR,sProjectDir);
			}
			else
			{
				iSetLogDirLoad = 0;
				strcpy(dirProject_name, "");
				strcpy(GsLogPath, "");
				SetCtrlVal(panel,PANEL_MODE_INDICATOR_SET_LOG_DIR,"");
				SetCtrlVal (panel, PANEL_MODE_LED3, 0);
				SetCtrlVal (panel, PANEL_MODE_LED12, 0x00000000); 
				return 0;
			}
			break;
			
	}
	return 0;
}

//***********************************************************************************************
// Modif - Carolina 02/2019
// Test with button See Graph - allows the user to see the graph of angular aceleration (trs/min)
// and pressure (bars) - for ergonomic
// - The button starts dimmed - enable after button execute
//***********************************************************************************************
int CVICALLBACK button_see_graph (int panel, int control, int event,
								  void *callbackData, int eventData1, int eventData2)
{
	int pnl=0, ctrl=0, quit=0, iErr = 0;
	//char message[100];
	//int seegraph_handle = GiGraphPanel;
	switch (event)
	{
		case EVENT_COMMIT:
			if ((GiGraphPanel = LoadPanelEx (0,"IhmModes.uir",PANELGRAPH, __CVIUserHInst)) < 0)
			{
				return -1;
			}

			flag_seegraph = 1;
			InstallPopup (GiGraphPanel);
			while (!quit)  //write condition when the execution finish
			{
				iErr = SetAxisScalingMode (GiGraphPanel, PANELGRAPH_GRAPH, VAL_BOTTOM_XAXIS, VAL_AUTOSCALE, 0.0, 100.0); //modif carolina
				if(iErr < 0)
				{
					MessagePopup ("Message", "This is a Message Popup");
				}

				/*if (!GetGiDisableMajGrapheVitesse())
				{
					GstGrapheMajGrapheVit(	GiGraphPanel, PANEL_MODE_TABLE_VITESSES, PANELGRAPH_GRAPH, 1,
											GstTabVitesse.dDuree, GstTabVitesse.dVit, GstTabVitesse.iNbElmts,
											1, iCOUL_CONS_VITESSE, 1);
				} */
				/*if (!GetGiDisableMajGraphePression())
				{
					GstGrapheMajGraphePres(GiGraphPanel, PANEL_MODE_TABLE_PRESSION, PANELGRAPH_GRAPH, 1,
										   GstTabPression.dDuree, GstTabPression.dPress, GstTabPression.iNbElmts,
										   1, iCOUL_CONS_PRESSION, 1);
				} */

				GetUserEvent (1, &pnl, &ctrl);

				if (ctrl == PANELGRAPH_QUITBUTTON )
				{
					//DiscardPanel (GiGraphPanel);
					HidePanel(GiGraphPanel);
					quit=1;
					flag_seegraph = 0;
				}
			}
			break;

	}
	return 0;

}

//***********************************************************************************************
// Modif - Carolina 02/2019
// Graph of the aceleration and pressure - view online
// Not finished
//***********************************************************************************************
int CVICALLBACK AffResult2 (int panel, int control, int event,
							void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{
		case EVENT_COMMIT:

			break;

		case EVENT_RIGHT_CLICK:

			break;
	}
	return 0;
}

//***********************************************************************************************
// Modif - Carolina 02/2019
// Table of the expected results
//MODIF MARVYN 18/06/2021 : load des expected results en même temps   
//***********************************************************************************************
int CVICALLBACK exp_results (int panel, int control, int event,
							 void *callbackData, int eventData1, int eventData2)
{
	Point cell;
	Rect  rect;
	BOOL bValid;

	int iError;
	int iX=0, iY=0, iNumberOfRows=0; //iInsertAfter=0;
	int iTop=0, iLeft=0;
	int iIdSelected=0;

	///////// Parametre Save
	//int iCellValue=0;
	char sCellValue[200]="";
	char sCellValue2[200]="";
	char range[30];

	/////////Parametre Load
	int boucle=0;
	char pathName[MAX_PATHNAME_LEN];
	char defaultDirectory[MAX_PATHNAME_LEN] = "D:\\";
	char currentDirectory[MAX_PATHNAME_LEN];
	int EndBoucle = 0, errDir = 0;

	////////move
	int numeroLigne=0;

	errDir = GetProjectDir(currentDirectory);
	if(errDir == -1 || errDir == -2)
	{
		FmtOut ("Project is untitled\n");
		strcpy(currentDirectory, defaultDirectory);
	}


	switch (event)
	{
		case EVENT_RIGHT_CLICK:
			if((GetGiStartCycleRun() == 0))
			{
				SetCtrlAttribute (panel, PANEL_MODE_EXP_RESULTS, ATTR_ENABLE_POPUP_MENU, 0);
				// Lecture de la position du panel (coordonnées en haut à gauche)
				GetPanelAttribute (panel, ATTR_LEFT, &iLeft);
				GetPanelAttribute (panel, ATTR_TOP, &iTop);

				iError = GstTablesGetRowColFromMouse(panel, control, &cell.y, &cell.x, &iX, &iY, &bValid);

				// Détermination du nombre de lignes du contrôle (data grid)
				iError = GetNumTableRows (panel, control, &iNumberOfRows);
				//NumRowsExpectedResults = iNumberOfRows;

				// Si le nombre de lignes est supérieur à zéro
				iError = GstTablesGstZoneSelect(panel, control, cell.y, cell.x,
												&rect.height, &rect.width, &rect.left,
												&rect.top, &bValid);

				//Menu selection
				iIdSelected = RunPopupMenu (GiMenuExpResultsHandle, MENUEXP_MAIN, panel,
											iY-iTop, iX-iLeft, 0, 0, 20, 20);
				
				switch(iIdSelected)
				{
					case MENUEXP_MAIN_ADD:
						//Open Script definition windows
						add_expected_results();
						insertSteps = 0 ;
						modifySteps =0;

						//Modif MaximePAGES 10/08/2020 ***************
						InstallCtrlCallback (GiExpectedResultsPanel, EXPRESULTS_VALUE, ValueTxt_Callback, 0);
						InstallCtrlCallback (GiExpectedResultsPanel, EXPRESULTS_TOLERENCE2, TolTxt_Callback, 0);
						InstallCtrlCallback (GiExpectedResultsPanel, EXPRESULTS_TOLERENCE1, TolPerTxt_Callback, 0);
						InstallCtrlCallback (GiExpectedResultsPanel, EXPRESULTS_VALUEFC, FunctionCodeTxt_Callback, 0);
						//********************************************
					

						break;
					//AJOUT Marvyn 	
					case MENUEXP_MAIN_INSERT:
						
							if(!CheckPrecondition())
						{
							return 0;
						}
					    //MARVYN 23/06/2021
						insertSteps = 1;
						modifySteps =0;
						numRow = rect.top ;
						if ( numRow != 0)
						{
						couleurSelection(numRow,PANEL_MODE_EXP_RESULTS,panel,VAL_LT_GRAY );
						//********
						add_expected_results();
				
					
						//Modif MaximePAGES 10/08/2020 ***************
						InstallCtrlCallback (GiExpectedResultsPanel, EXPRESULTS_VALUE, ValueTxt_Callback, 0);
						InstallCtrlCallback (GiExpectedResultsPanel, EXPRESULTS_TOLERENCE2, TolTxt_Callback, 0);
						InstallCtrlCallback (GiExpectedResultsPanel, EXPRESULTS_TOLERENCE1, TolPerTxt_Callback, 0);
						InstallCtrlCallback (GiExpectedResultsPanel, EXPRESULTS_VALUEFC, FunctionCodeTxt_Callback, 0);
						}
					break;	
					//AJOUT Marvyn
					case MENUEXP_MAIN_MODIFY:
						//Open Script definition windows
						if(!CheckPrecondition())
						{
							return 0;
						}
						numRow = rect.top ;
						tableHeight = rect.height ;
						modifySteps =1;
						insertSteps = 0; 
						if ( numRow != 0)
						{
						couleurSelection(numRow,PANEL_MODE_EXP_RESULTS, panel,VAL_LT_GRAY )  ;
						add_expected_results();
						//MARVYN
						modifyStepExpResult(numRow);
						
						//Modif MaximePAGES 10/08/2020 ***************
						InstallCtrlCallback (GiExpectedResultsPanel, EXPRESULTS_VALUE, ValueTxt_Callback, 0);
						InstallCtrlCallback (GiExpectedResultsPanel, EXPRESULTS_TOLERENCE2, TolTxt_Callback, 0);
						InstallCtrlCallback (GiExpectedResultsPanel, EXPRESULTS_TOLERENCE1, TolPerTxt_Callback, 0);
						InstallCtrlCallback (GiExpectedResultsPanel, EXPRESULTS_VALUEFC, FunctionCodeTxt_Callback, 0);
						//********************************************
						}
					break; 

					case MENUEXP_MAIN_DELETE:
						if(iNumberOfRows == 0)
						{
							return 0;
						}
						else
						{
							//printf("%d %d \n",rect.top, rect.height);
							//DeleteTableRows (panelHandle, tableCtrl, index, numToDelete); //Programming with Table Controls
							DeleteTableRows (GiPanel, PANEL_MODE_EXP_RESULTS, rect.top, rect.height);
						}
						if(*PointeurNbRowsExpR > 0)
						{
							*PointeurNbRowsExpR= *PointeurNbRowsExpR-1;
						}
						break;

					case MENUEXP_MAIN_DELETEALL:
						if(iNumberOfRows == 0)
						{
							return 0;
						}
						else
						{
							if(ConfirmPopup ("Delete All", "Do you want to delete all lines?"))
							{
								DeleteTableRows (GiPanel, PANEL_MODE_EXP_RESULTS, 1, -1);
								*PointeurNbRowsExpR=0;	// remet a 0 le compteur de ligne du tableau
							}
						}

						break;

					case MENUEXP_MAIN_MOVEUP:
						numeroLigne = rect.top;
						if(numeroLigne<2)
						{
						}
						else
						{
							for(boucle=1; boucle < 9 ; boucle++)  //6 column
							{
								cell.x=boucle;
								cell.y =numeroLigne;
								GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
								cell.y =numeroLigne-1;
								GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue2);
								SetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
								cell.y =numeroLigne;
								SetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue2);
							}
						}

						break;

					case MENUEXP_MAIN_MOVEDOWN:
						numeroLigne = rect.top;
						if(numeroLigne==*PointeurNbRowsExpR)
						{
						}
						else
						{
							for(boucle=1; boucle < 9 ; boucle++)
							{
								cell.x=boucle;
								cell.y =numeroLigne;
								GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
								cell.y =numeroLigne+1;
								GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue2);
								SetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
								cell.y =numeroLigne;
								SetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue2);
							}

						}
						

						break;
						
					case MENUEXP_MAIN_LOADEXPECTED:
						FileSelectPopupEx (currentDirectory, "*.xlsx", "*.xlsx", "Load script file", VAL_LOAD_BUTTON, 0, 1, pathName);
						//FileSelectPopup(currentDirectory,"*.xlsx","*.xlsx", "Load script file", VAL_LOAD_BUTTON, 0, 1, 1, 1, pathName);
						// Ouverture d'excel pour charger le fichier
						//ExcelRpt_ApplicationNew(VFALSE,&applicationHandleLoad);
						ExcelRpt_WorkbookOpen (applicationHandleProject, pathName, &workbookHandleLoad);
						ExcelRpt_GetWorksheetFromIndex(workbookHandleLoad, 2, &worksheetHandleLoad);
						ExcelRpt_GetCellValue (worksheetHandleLoad, "I1", CAVT_INT,&EndBoucle);

						//printf("pathname2 = %s\n",pathName);
						//printf("pathname = %d\n",pathName[strlen(pathName)-1]);
						if (strcmp(pathName,"")!=0)
						{
						if ( pathName[strlen(pathName)-1] != 25  ) 
						{
						DeleteTableRows (panel, PANEL_MODE_SCRIPTS_DETAILS, 1, -1);
						*pointeurval=0;	// remet a 0 le compteur de ligne du tableau
							
					
						DeleteTableRows (GiPanel, PANEL_MODE_EXP_RESULTS, 1, -1);
						*PointeurNbRowsExpR=0;	// remet a 0 le compteur de ligne du tableau
										  
						
						*PointeurNbRowsExpR= EndBoucle-1;

						// charge chaque lignes du tableau
						for(boucle=1; boucle <= EndBoucle-1; boucle++)
						{

							InsertTableRows (GiPanel, PANEL_MODE_EXP_RESULTS, -1, 1, VAL_USE_MASTER_CELL_TYPE);

							cell.y =boucle;
							//x number of columns
							cell.x =1;  //Command check
							sprintf(range,"A%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandleLoad, range, CAVT_CSTRING,sCellValue);
							SetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);

							cell.x =2;  //Function code value
							sprintf(range,"B%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandleLoad,range , CAVT_CSTRING,sCellValue);
							SetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);

							cell.x =3;  //labels
							sprintf(range,"C%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandleLoad,range , CAVT_CSTRING,sCellValue);
							SetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);

							cell.x =4;  //interface
							sprintf(range,"D%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandleLoad,range , CAVT_CSTRING,sCellValue);
							SetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);

							cell.x =5;  //parameters
							sprintf(range,"E%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandleLoad,range , CAVT_CSTRING,sCellValue);
							SetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);

							cell.x =6;  //Value
							sprintf(range,"F%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandleLoad,range , CAVT_CSTRING,sCellValue);
							SetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);

							cell.x =7;  //tolerence
							sprintf(range,"G%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandleLoad,range , CAVT_CSTRING,sCellValue);
							SetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);

							cell.x =8;  //tolerence %
							sprintf(range,"H%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandleLoad,range , CAVT_CSTRING,sCellValue);
							SetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);

							//	couleurLigne(boucle, panel);
						}
						
					//	ExcelRpt_GetWorksheetFromIndex(workbookHandleLoad, 1, &worksheetHandleLoad);
					//	ExcelRpt_GetCellValue (worksheetHandleLoad, "I1", CAVT_INT,&EndBoucle);
						ExcelRpt_WorkbookClose (workbookHandleLoad, 0);
						ExcelRpt_WorkbookOpen (applicationHandleProject, pathName, &workbookHandle7);
						ExcelRpt_GetWorksheetFromIndex(workbookHandle7, 1, &worksheetHandle7);
						ExcelRpt_GetCellValue (worksheetHandle7, "I1", CAVT_INT,&EndBoucle);

						*pointeurval= EndBoucle-1;

						// charge chaque lignes du tableau
						for(boucle=1; boucle <= EndBoucle-1; boucle++)
						{

							InsertTableRows (panel, PANEL_MODE_SCRIPTS_DETAILS, -1, 1, VAL_USE_MASTER_CELL_TYPE);

							cell.y =boucle;
							//x number of columns
							cell.x =1;  //Time
							sprintf(range,"A%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandle7, range, CAVT_CSTRING,sCellValue);
							SetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);

							cell.x =2;  //Duration
							sprintf(range,"B%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandle7,range , CAVT_CSTRING,sCellValue);
							SetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);

							cell.x =3;  //Function
							sprintf(range,"C%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandle7,range , CAVT_CSTRING,sCellValue);
							SetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);

							cell.x =4;  //Value
							sprintf(range,"D%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandle7,range , CAVT_CSTRING,sCellValue);
							SetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);

							cell.x =5;  //Nb of Frames
							sprintf(range,"E%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandle7,range , CAVT_CSTRING,sCellValue);
							SetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);

							cell.x =6;  //WU ID
							sprintf(range,"F%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandle7,range , CAVT_CSTRING,sCellValue);
							SetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);

							cell.x =7;  //Interframe
							sprintf(range,"G%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandle7,range , CAVT_CSTRING,sCellValue);
							SetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);

							cell.x =8;  //Comments
							sprintf(range,"H%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandle7,range , CAVT_CSTRING,sCellValue);
							SetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);

							couleurLigne(boucle, panel);
						}

						/*cell.x =1; //END line
						sprintf(range,"A%d",boucle+1);
						ExcelRpt_GetCellValue (worksheetHandle3, range, CAVT_INT,&iCellValue);
						SetCtrlVal(panel,PANEL_MODE_ENDTIME,iCellValue); */
						}
						}
						
						ExcelRpt_WorkbookClose (workbookHandle7, 0);
						//ExcelRpt_ApplicationQuit (applicationHandle7);
						//CA_DiscardObjHandle(applicationHandle7);


						break;

				}

			}
	}
	// Fermeture excel
	//ExcelRpt_WorkbookClose (workbookHandleLoad, 0);
	//ExcelRpt_ApplicationQuit (applicationHandleLoad);
	//CA_DiscardObjHandle(applicationHandleLoad);

	return 0;
}

//***********************************************************************************************
// Modif - Carolina 02/2019
// This mode shows two tables : Test Script and Expected results
//if initially hidden:	0 visible
//					  	1 invisible
//***********************************************************************************************
int CVICALLBACK mode_creation (int panel, int control, int event,
							   void *callbackData, int eventData1, int eventData2)
{
	

	switch (event)
	{
			//	enable_create = 1;
		case EVENT_COMMIT:
			
			mode = 0;
			
			//VAL_GREEN Red = 0, Green = 255, Blue = 0
			//SetCtrlAttribute (GiPanel,PANEL_MODE_MODE_CREATION, ATTR_CMD_BUTTON_COLOR,0x003399FF ); //VAL_BLUE
			//SetCtrlAttribute (GiPanel,PANEL_MODE_MODE_EXE, ATTR_CMD_BUTTON_COLOR,0x00E0E0E0);
			SetCtrlVal(GiPanel,PANEL_MODE_MODE_CREATION,1);
			SetCtrlVal(GiPanel,PANEL_MODE_MODE_EXE,0);
			SetCtrlVal(GiPanel,PANEL_MODE_MODE_ANALYSE,0); 
			
			//Make visible tables : Script sequence and Test Script 
			SetCtrlAttribute (GiPanel, PANEL_MODE_EXP_RESULTS, 		ATTR_VISIBLE ,1);
			SetCtrlAttribute (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, 	ATTR_VISIBLE ,1);
			SetCtrlAttribute (GiPanel, PANEL_MODE_DECORATION_2, 		ATTR_VISIBLE ,1);
			SetCtrlAttribute (GiPanel, PANEL_MODE_DECORATION_3, 		ATTR_VISIBLE ,1);
			SetCtrlAttribute (GiPanel, PANEL_MODE_SCRIPTS_DETAILS_2, 	ATTR_VISIBLE ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_TABLE_SCRIPT, 		ATTR_VISIBLE ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_PICTURE, 			ATTR_VISIBLE ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_DECORATION_5, 		ATTR_VISIBLE ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_SHOW_ACC, 		ATTR_DIMMED ,1);
			SetCtrlAttribute (GiPanel, PANEL_MODE_SHOW_PRESS, 		ATTR_DIMMED ,1);
			SetCtrlAttribute (GiPanel, PANEL_MODE_DECORATION_8, 		ATTR_DIMMED ,1);
			SetCtrlAttribute (GiPanel, PANEL_MODE_TEXTMSG_9, 		ATTR_DIMMED ,1);
			SetCtrlAttribute (GiPanel, PANEL_MODE_TEXTMSG_8, 		ATTR_DIMMED ,1);
			SetCtrlAttribute (GiPanel, PANEL_MODE_DECORATION_6, 		ATTR_VISIBLE ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_EXPRESULTSC, 		ATTR_VISIBLE ,1);
			SetCtrlAttribute (GiPanel, PANEL_MODE_TESTSCRIPT, 			ATTR_VISIBLE ,1);
			SetCtrlAttribute (GiPanel, PANEL_MODE_SCRIPTSEQEXE, 		ATTR_VISIBLE ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_ONLINESEQ, 			ATTR_VISIBLE ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_ENDTIME, 			ATTR_VISIBLE ,1);
			//SetCtrlAttribute (GiPanel, PANEL_MODE_SAVEFILES, 			ATTR_VISIBLE ,1);
			//SetCtrlAttribute (GiPanel, PANEL_MODE_SAVEFILES,  			ATTR_VISIBLE, 1);
			//SetCtrlAttribute (GiPanel, PANEL_MODE_SAVEFILES,  			ATTR_DIMMED, 0);
			//SetCtrlAttribute (GiPanel, PANEL_MODE_SAVEFILEBUTTON,   	ATTR_VISIBLE, 1);
			//SetCtrlAttribute (GiPanel, PANEL_MODE_SAVEFILEBUTTON,  	ATTR_DIMMED, 0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_BUT_START_RUN,  		ATTR_DIMMED, 1);
			SetCtrlAttribute (GiPanel, PANEL_MODE_TOTALTIME, 			ATTR_VISIBLE ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_ENDTIMESEQUENCE, 	ATTR_VISIBLE ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_ENDTIMESEQUENCE, 	ATTR_VISIBLE ,0);
			SetCtrlAttribute(GiPanel, PANEL_MODE_PROGRESSBAR, 		ATTR_VISIBLE, 0);
			SetCtrlAttribute(GiPanel, PANEL_MODE_PROGRESSBARSEQ, 		ATTR_VISIBLE, 0);
			//SetCtrlAttribute(GiPanel, PANEL_MODE_FAILEDSEQUENCE, 		ATTR_VISIBLE, 0);

			SetCtrlAttribute (GiPanel, PANEL_MODE_SAVEFILEBUTTON,   	ATTR_DIMMED, 0); 
			SetCtrlAttribute (GiPanel, PANEL_MODE_SAVEFILEBUTTON, ATTR_VISIBLE, 	1); //AJOUT MaximePAGES - 15/06/2020
			
			
			SetCtrlAttribute (GiPanel, PANEL_MODE_INDICATOR_TESTSCRIPT, 	ATTR_VISIBLE ,0);  
			SetCtrlAttribute (GiPanel, PANEL_MODE_INDICATOR_TESTFOLDER, 	ATTR_VISIBLE ,0);  
			SetCtrlAttribute (GiPanel, PANEL_MODE_BUTTON_TESTSCRIPT, 		ATTR_VISIBLE ,0);  
			SetCtrlAttribute (GiPanel, PANEL_MODE_BUTTON_TESTFOLDER, 		ATTR_VISIBLE ,0);   
			SetCtrlAttribute (GiPanel, PANEL_MODE_BUTTON_ANALYSE, 			ATTR_VISIBLE ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_TEXTBOX_ANALYSIS, 		ATTR_VISIBLE ,0); 
			SetCtrlAttribute (GiPanel, PANEL_MODE_CHECKRESULTS, 			ATTR_VISIBLE ,0);
			

			break;
	}
	return 0;
}

//***********************************************************************************************
// Modif - Carolina 02/2019
// This mode shows two tables : Script sequence and Test Script
//if initially hidden:	0 visible
//					  	1 invisible
//***********************************************************************************************
int CVICALLBACK mode_exe (int panel, int control, int event,
						  void *callbackData, int eventData1, int eventData2)
{
	switch (event)				   //0 invisible, 1 visible
	{
			//0 enable
			//	enable_exe = 2;
		case EVENT_COMMIT:

			mode = 1; 
			
			

			//SetCtrlAttribute (GiPanel,PANEL_MODE_MODE_EXE, ATTR_CMD_BUTTON_COLOR,0x003399FF ); //VAL_BLUE
			//SetCtrlAttribute (GiPanel,PANEL_MODE_MODE_CREATION, ATTR_CMD_BUTTON_COLOR,0x00E0E0E0);
			
			SetCtrlVal(GiPanel,PANEL_MODE_MODE_CREATION,0);
			SetCtrlVal(GiPanel,PANEL_MODE_MODE_EXE,1); 
			SetCtrlVal(GiPanel,PANEL_MODE_MODE_ANALYSE,0);



			//Make visible tables : Test Script and Expected results
			SetCtrlAttribute (GiPanel, PANEL_MODE_SCRIPTS_DETAILS_2, 	ATTR_VISIBLE ,1);
			SetCtrlAttribute (GiPanel, PANEL_MODE_TABLE_SCRIPT, 		ATTR_VISIBLE ,1);
			SetCtrlAttribute (GiPanel, PANEL_MODE_DECORATION_2, 		ATTR_VISIBLE ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_DECORATION_3, 		ATTR_VISIBLE ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_EXP_RESULTS, 		ATTR_VISIBLE ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, 	ATTR_VISIBLE ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_PICTURE, 			ATTR_VISIBLE ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_DECORATION_8, 		ATTR_DIMMED ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_SHOW_PRESS, 		ATTR_DIMMED ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_SHOW_ACC, 		ATTR_DIMMED ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_TEXTMSG_9, 		ATTR_DIMMED ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_TEXTMSG_8, 		ATTR_DIMMED ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_DECORATION_5, 		ATTR_VISIBLE ,1);
			SetCtrlAttribute (GiPanel, PANEL_MODE_DECORATION_6, 		ATTR_VISIBLE ,1);
			SetCtrlAttribute (GiPanel, PANEL_MODE_SCRIPTSEQEXE, 		ATTR_VISIBLE ,1);
			SetCtrlAttribute (GiPanel, PANEL_MODE_ONLINESEQ, 			ATTR_VISIBLE ,1);
			SetCtrlAttribute (GiPanel, PANEL_MODE_SHOW, 			ATTR_VISIBLE ,1); 
			SetCtrlAttribute (GiPanel, PANEL_MODE_SHOW, 			ATTR_DIMMED ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_EXPRESULTSC, 		ATTR_VISIBLE ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_TESTSCRIPT, 			ATTR_VISIBLE ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_ENDTIME, 			ATTR_VISIBLE ,0);
			//SetCtrlAttribute (GiPanel, PANEL_MODE_SAVEFILES, 			ATTR_VISIBLE ,0);
			//SetCtrlAttribute (GiPanel, PANEL_MODE_SAVEFILES,  			ATTR_VISIBLE, 0);
			//SetCtrlAttribute (GiPanel, PANEL_MODE_SAVEFILES,  			ATTR_DIMMED,1);
			//SetCtrlAttribute (GiPanel, PANEL_MODE_SAVEFILEBUTTON,  	ATTR_VISIBLE, 0);
			//SetCtrlAttribute (GiPanel, PANEL_MODE_SAVEFILEBUTTON,  	ATTR_DIMMED,1);  //mise en com par MaximePAGES
			SetCtrlAttribute (GiPanel, PANEL_MODE_BUT_START_RUN,  		ATTR_DIMMED,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_TOTALTIME, 			ATTR_VISIBLE ,1);
			SetCtrlAttribute (GiPanel, PANEL_MODE_ENDTIMESEQUENCE, 	ATTR_VISIBLE ,1);
			SetCtrlAttribute(GiPanel, PANEL_MODE_PROGRESSBAR, 		ATTR_VISIBLE, 1);
			SetCtrlAttribute(GiPanel, PANEL_MODE_PROGRESSBARSEQ, 		ATTR_VISIBLE, 1);
			//SetCtrlAttribute(GiPanel, PANEL_MODE_FAILEDSEQUENCE, 		ATTR_VISIBLE, 1);

			SetCtrlAttribute (GiPanel, PANEL_MODE_SAVEFILEBUTTON,   	ATTR_DIMMED, 0); 
			SetCtrlAttribute (GiPanel, PANEL_MODE_SAVEFILEBUTTON,   	ATTR_VISIBLE, 1);
			
			
			SetCtrlAttribute (GiPanel, PANEL_MODE_INDICATOR_TESTSCRIPT, 	ATTR_VISIBLE ,0);  
			SetCtrlAttribute (GiPanel, PANEL_MODE_INDICATOR_TESTFOLDER, 	ATTR_VISIBLE ,0);  
			SetCtrlAttribute (GiPanel, PANEL_MODE_BUTTON_TESTSCRIPT, 		ATTR_VISIBLE ,0);  
			SetCtrlAttribute (GiPanel, PANEL_MODE_BUTTON_TESTFOLDER, 		ATTR_VISIBLE ,0);   
			SetCtrlAttribute (GiPanel, PANEL_MODE_BUTTON_ANALYSE, 			ATTR_VISIBLE ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_TEXTBOX_ANALYSIS, 		ATTR_VISIBLE ,0); 
			SetCtrlAttribute (GiPanel, PANEL_MODE_CHECKRESULTS, 			ATTR_VISIBLE ,0); 

			break;
	}
	return 0;
}

//***********************************************************************************************
// Modif - Carolina 02/2019
// Panel EXPECTED RESULTS
// This panel allows the user building the expected results table
//***********************************************************************************************
int CVICALLBACK expected_list (int panel, int control, int event,
							   void *callbackData, int eventData1, int eventData2)
{
	int itemValue= 0;
	int iparamdata=0, iselectparam=0, ifieldcheck=0;



	switch (event)
	{
			
	
		case EVENT_VAL_CHANGED:
			
		
			
			GetCtrlVal(panel,EXPRESULTS_EXPLIST, &itemValue);

			// remise a zero des indicateur
			//	DeleteListItem (GiExpectedResultsPanel, EXPRESULTS_FIELDCHECK, 0, -1);
			//	DeleteListItem (GiExpectedResultsPanel, EXPRESULTS_SELECTPARAM, 0, -1);

			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA,iparamdata);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,ifieldcheck);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_ATTRIBUTENAME,"");
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_SELECTPARAM,iselectparam);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,"");
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_EXPUNIT,"");
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_EXPVALUE,"");
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_VALUE,"");

			dimmedNumericKeypadForExp(1); //Modif MaximePAGES 30/07/2020 - New IHM Numeric Keypad
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,"");
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_EXPVALUE_2,0.0);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_EXPVALUE,"");
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_EXPUNIT,"");

			/*
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_SELECTPARAM,		ATTR_VISIBLE, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,		ATTR_VISIBLE, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_INSERTFORMULE,	ATTR_VISIBLE, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_7,		ATTR_VISIBLE, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DECORATION_5,	ATTR_VISIBLE, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTPAR2,			ATTR_VISIBLE, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTPAR1,			ATTR_VISIBLE, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTSUM,			ATTR_VISIBLE, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTSUB,			ATTR_VISIBLE, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTMULT,			ATTR_VISIBLE, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTDIV,			ATTR_VISIBLE, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_EXPUNIT,			ATTR_VISIBLE, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_EXPVALUE,		ATTR_VISIBLE, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_EXPVALUE_2,		ATTR_VISIBLE, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_8,		ATTR_VISIBLE, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DELETEBUTTON,	ATTR_VISIBLE, 0);
			//SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DELVALUE,		ATTR_VISIBLE, 0);
			*/

			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_BTNTOLVAL,1); //Modif MaximePAGES 03/07/2020 E205
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_BTNTOLPRCT,0); //Modif MaximePAGES 03/07/2020 E205

			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DECORATION_7,	ATTR_VISIBLE, 0); //Modif MaximePAGES 10/07/2020
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTINCH,		ATTR_VISIBLE, 0); //Modif MaximePAGES 10/07/2020
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_WHEELDIM,		ATTR_VISIBLE, 0); //Modif MaximePAGES 10/07/2020

			//Modif MaximePAGES 13/08/2020- WU ID selection in IHM
			SetCtrlAttribute(panel,EXPRESULTS_DECORATION_8,			ATTR_DIMMED,0); 
			SetCtrlAttribute(panel,EXPRESULTS_DECORATION_9,			ATTR_DIMMED,0); 
			//SetCtrlAttribute(panel,EXPRESULTS_WUIDEXP,				ATTR_DIMMED,0); 
			//SetCtrlAttribute(panel,EXPRESULTS_WUID_VALUE,			ATTR_DIMMED,0); 
			
			

			switch (itemValue)
			{

				case 0:

					//Select - initial panel - all dimmed
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_LABEL1,			ATTR_DIMMED,1);
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_LABEL2,			ATTR_DIMMED,1);
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_VALUE,			ATTR_DIMMED,1);
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TXTUNITLABEL,	ATTR_DIMMED,1);
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG,			ATTR_DIMMED,1);
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE1,		ATTR_DIMMED,1);
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE2,		ATTR_DIMMED,1);
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDEXP,			ATTR_DIMMED,1);
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDTOL,			ATTR_DIMMED,1);
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDTOL_2,		ATTR_DIMMED,1);
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDVAL,			ATTR_DIMMED,1);
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,		ATTR_DIMMED,1);
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA,	ATTR_DIMMED,1);
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_VALUEFC,			ATTR_DIMMED,1);
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DELVALUE,		ATTR_DIMMED,1);
					SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_TXTUNITBODY, ""); //Modif MaximePAGES 03/07/2020 E206
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BTNTOLVAL,		ATTR_DIMMED,1); //Modif MaximePAGES 03/07/2020 E205
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BTNTOLPRCT,		ATTR_DIMMED,1); //Modif MaximePAGES 03/07/2020 E205
					SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_BTNTOLVAL,1);  //Modif MaximePAGES 03/07/2020 E205
					SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_BTNTOLPRCT,0); //Modif MaximePAGES 03/07/2020 E205

					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG_FIXEDVALUE,ATTR_DIMMED,1); //Modif MaximePAGES 06/07/2020 E207
					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG_SEQVALUE,	 ATTR_DIMMED,1); //Modif MaximePAGES 06/07/2020 E207
					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG_RANGEVALUE,ATTR_DIMMED,1); //Modif MaximePAGES 06/07/2020 E207

					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DECORATION_7,	ATTR_VISIBLE, 0); //Modif MaximePAGES 10/07/2020
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTINCH,		ATTR_VISIBLE, 0); //Modif MaximePAGES 10/07/2020
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_WHEELDIM,		ATTR_VISIBLE, 0); //Modif MaximePAGES 10/07/2020

						//Modif MaximePAGES 13/08/2020- WU ID selection in IHM        
					SetCtrlAttribute(panel,EXPRESULTS_DECORATION_8,			ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_DECORATION_9,			ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_WUIDEXP,				ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_WUID_VALUE,			ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_ATTRIBUTENAME,	ATTR_DIMMED,1);

					insert_parameter = 0;
				

					if (insert_parameter==0)
					{
						//invisible the "insert parameters"
						//visible the parameters

						dimmedNumericKeypadForExp(1); //Modif MaximePAGES 30/07/2020 - New IHM Numeric Keypad

						/*
						SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_SELECTPARAM,		ATTR_VISIBLE, 0);
						SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,		ATTR_VISIBLE, 0);
						SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_INSERTFORMULE,	ATTR_VISIBLE, 0);
						SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_7,		ATTR_VISIBLE, 0);
						SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DECORATION_5,	ATTR_VISIBLE, 0);
						SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTPAR2,			ATTR_VISIBLE, 0);
						SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTPAR1,			ATTR_VISIBLE, 0);
						SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTSUM,			ATTR_VISIBLE, 0);
						SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTSUB,			ATTR_VISIBLE, 0);
						SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTMULT,			ATTR_VISIBLE, 0);
						SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTDIV,			ATTR_VISIBLE, 0);
						SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_EXPUNIT,			ATTR_VISIBLE, 0);
						SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_EXPVALUE,		ATTR_VISIBLE, 0);
						SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_EXPVALUE_2,		ATTR_VISIBLE, 0);
						SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_8,		ATTR_VISIBLE, 0);
						SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DELETEBUTTON,	ATTR_VISIBLE, 0);
						*/

					}

					break;
				case 1:   //CheckTimingInterFrame - verifier l'interframe

					
					SetCtrlAttribute(panel,EXPRESULTS_LABEL1,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_LABEL2,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_VALUE,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_TXTUNITLABEL,	ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_ADDVAL,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_DELVALUE,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_TOLERENCE1,		ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_ADDTOL,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_TOLERENCE2,		ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_ADDTOL_2,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_ADDEXP,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_PARAMETERSDATA,	ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_VALUEFC,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_FIELDCHECK,		ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_ATTRIBUTENAME,	ATTR_DIMMED,1); 

					SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_TXTUNITBODY, "[s]"); //Modif MaximePAGES 03/07/2020 E206
					SetCtrlAttribute(panel,EXPRESULTS_BTNTOLVAL,		ATTR_DIMMED,0); //Modif MaximePAGES 03/07/2020 E205
					SetCtrlAttribute(panel,EXPRESULTS_BTNTOLPRCT,		ATTR_DIMMED,0); //Modif MaximePAGES 03/07/2020 E205

					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG_FIXEDVALUE,ATTR_DIMMED,0); //Modif MaximePAGES 06/07/2020 E207
					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG_SEQVALUE,	 ATTR_DIMMED,0); //Modif MaximePAGES 06/07/2020 E207
					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG_RANGEVALUE,ATTR_DIMMED,1); //Modif MaximePAGES 06/07/2020 E207

					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DECORATION_7,	ATTR_VISIBLE, 0); //Modif MaximePAGES 10/07/2020
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTINCH,		ATTR_VISIBLE, 0); //Modif MaximePAGES 10/07/2020
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_WHEELDIM,		ATTR_VISIBLE, 0); //Modif MaximePAGES 10/07/2020
					//SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA,	ATTR_DIMMED,1); // MARVYN 02/07/2021
					
					insert_parameter = 1;
					fieldNeeded = 0;
					
					break;
				case 2: //CheckTimingInterBursts - verifier l'interburst
					SetCtrlAttribute(panel,EXPRESULTS_LABEL1,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_LABEL2,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_VALUE,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_TXTUNITLABEL,	ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_ADDVAL,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_DELVALUE,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_TOLERENCE1,		ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_ADDTOL,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_TOLERENCE2,		ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_ADDTOL_2,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_ADDEXP,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_PARAMETERSDATA,	ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_VALUEFC,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_FIELDCHECK,		ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_ATTRIBUTENAME,	ATTR_DIMMED,1);
					
					SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_TXTUNITBODY, "[s]"); //Modif MaximePAGES 03/07/2020 E206
					SetCtrlAttribute(panel,EXPRESULTS_BTNTOLVAL,		ATTR_DIMMED,0); //Modif MaximePAGES 03/07/2020 E205
					SetCtrlAttribute(panel,EXPRESULTS_BTNTOLPRCT,		ATTR_DIMMED,0); //Modif MaximePAGES 03/07/2020 E205

					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG_FIXEDVALUE,ATTR_DIMMED,0); //Modif MaximePAGES 06/07/2020 E207
					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG_SEQVALUE,	 ATTR_DIMMED,1); //Modif MaximePAGES 06/07/2020 E207
					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG_RANGEVALUE,ATTR_DIMMED,1); //Modif MaximePAGES 06/07/2020 E207

					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DECORATION_7,	ATTR_VISIBLE, 0); //Modif MaximePAGES 10/07/2020
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTINCH,		ATTR_VISIBLE, 0); //Modif MaximePAGES 10/07/2020
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_WHEELDIM,		ATTR_VISIBLE, 0); //Modif MaximePAGES 10/07/2020
				//	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA,	ATTR_DIMMED,1);  // MARVYN 02/07/2021
					insert_parameter = 1;
					fieldNeeded =  0;
					
					break;
				case 3: //CheckFieldValue - verifier la valeur d'un champ
					SetCtrlAttribute(panel,EXPRESULTS_LABEL1,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_LABEL2,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_VALUE,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_TXTUNITLABEL,		ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_ADDVAL,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_DELVALUE,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_TOLERENCE1,		ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_ADDTOL,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_ADDTOL_2,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_TOLERENCE2,		ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_ADDEXP,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_PARAMETERSDATA,	ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_VALUEFC,			ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_FIELDCHECK,		ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_ATTRIBUTENAME,	ATTR_DIMMED,1);

					SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_TXTUNITBODY, "[s,g,kPa...]"); //Modif MaximePAGES 03/07/2020 E206
					SetCtrlAttribute(panel,EXPRESULTS_BTNTOLVAL,		ATTR_DIMMED,0); //Modif MaximePAGES 03/07/2020 E205
					SetCtrlAttribute(panel,EXPRESULTS_BTNTOLPRCT,		ATTR_DIMMED,0); //Modif MaximePAGES 03/07/2020 E205

					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG_FIXEDVALUE,ATTR_DIMMED,0); //Modif MaximePAGES 06/07/2020 E207
					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG_SEQVALUE,	 ATTR_DIMMED,0); //Modif MaximePAGES 06/07/2020 E207
					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG_RANGEVALUE,ATTR_DIMMED,0); //Modif MaximePAGES 06/07/2020 E207

					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DECORATION_7,	ATTR_VISIBLE, 0); //Modif MaximePAGES 10/07/2020
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTINCH,		ATTR_VISIBLE, 0); //Modif MaximePAGES 10/07/2020
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_WHEELDIM,		ATTR_VISIBLE, 0); //Modif MaximePAGES 10/07/2020


					insert_parameter = 1;
					fieldNeeded =  1;
					break;
				case 4: //CheckSTDEV - verifier l'ecart type des valeurs des angles
					SetCtrlAttribute(panel,EXPRESULTS_VALUEFC,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_LABEL1,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_LABEL2,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_VALUE,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_TXTUNITLABEL,		ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_ADDVAL,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_DELVALUE,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_TOLERENCE1,		ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_TOLERENCE2,		ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_ADDTOL,			ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_ADDTOL_2,			ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_ADDEXP,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_PARAMETERSDATA,	ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_FIELDCHECK,		ATTR_DIMMED,1);
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA,	ATTR_DIMMED,1);
					insert_parameter = 1;
					fieldNeeded =  0; 
					SetCtrlAttribute(panel,EXPRESULTS_BTNTOLVAL,		ATTR_DIMMED,1); //Modif MaximePAGES 03/07/2020 E205
					SetCtrlAttribute(panel,EXPRESULTS_BTNTOLPRCT,		ATTR_DIMMED,1); //Modif MaximePAGES 03/07/2020 E205
					SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_TXTUNITBODY, "[degree]"); //Modif MaximePAGES 03/07/2020 E206
					SetCtrlAttribute(panel,EXPRESULTS_ATTRIBUTENAME,	ATTR_DIMMED,1);
					
					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG_FIXEDVALUE,ATTR_DIMMED,1); //Modif MaximePAGES 06/07/2020 E207
					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG_SEQVALUE,	 ATTR_DIMMED,1); //Modif MaximePAGES 06/07/2020 E207
					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG_RANGEVALUE,ATTR_DIMMED,0); //Modif MaximePAGES 06/07/2020 E207

					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DECORATION_7,	ATTR_VISIBLE, 0); //Modif MaximePAGES 10/07/2020
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTINCH,		ATTR_VISIBLE, 0); //Modif MaximePAGES 10/07/2020
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_WHEELDIM,		ATTR_VISIBLE, 0); //Modif MaximePAGES 10/07/2020


					break;
				case 5: //CheckNbBursts - verifier le nombre de burst
					SetCtrlAttribute(panel,EXPRESULTS_LABEL1,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_LABEL2,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_VALUE,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_TXTUNITLABEL,		ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_ADDVAL,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_DELVALUE,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_TOLERENCE1,		ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_TOLERENCE2,		ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_ADDTOL,			ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_ADDTOL_2,			ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_ADDEXP,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_PARAMETERSDATA,	ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_VALUEFC,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_FIELDCHECK,		ATTR_DIMMED,1);
				//	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA,	ATTR_DIMMED,1);   
					insert_parameter = 1;
					fieldNeeded =  0;
					SetCtrlAttribute(panel,EXPRESULTS_BTNTOLVAL,		ATTR_DIMMED,1); //Modif MaximePAGES 03/07/2020 E205
					SetCtrlAttribute(panel,EXPRESULTS_BTNTOLPRCT,		ATTR_DIMMED,1); //Modif MaximePAGES 03/07/2020 E205
					SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_TXTUNITBODY, "bursts"); //Modif MaximePAGES 03/07/2020 E206
					SetCtrlAttribute(panel,EXPRESULTS_ATTRIBUTENAME,	ATTR_DIMMED,1);
					
					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG_FIXEDVALUE,ATTR_DIMMED,0); //Modif MaximePAGES 06/07/2020 E207
					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG_SEQVALUE,	 ATTR_DIMMED,1); //Modif MaximePAGES 06/07/2020 E207
					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG_RANGEVALUE,ATTR_DIMMED,1); //Modif MaximePAGES 06/07/2020 E207

					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DECORATION_7,	ATTR_VISIBLE, 0); //Modif MaximePAGES 10/07/2020
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTINCH,		ATTR_VISIBLE, 0); //Modif MaximePAGES 10/07/2020
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_WHEELDIM,		ATTR_VISIBLE, 0); //Modif MaximePAGES 10/07/2020


					break;
				case 6: // CheckNbFramesInBurst - verifier le nombre de trames dans un burst
					SetCtrlAttribute(panel,EXPRESULTS_LABEL1,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_LABEL2,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_VALUE,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_TXTUNITLABEL,		ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_ADDVAL,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_DELVALUE,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_TOLERENCE1,		ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_TOLERENCE2,		ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_ADDTOL,			ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_ADDTOL_2,			ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_ADDEXP,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_PARAMETERSDATA,	ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_VALUEFC,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_FIELDCHECK,		ATTR_DIMMED,1);
					insert_parameter = 1;
					fieldNeeded =  0;
					SetCtrlAttribute(panel,EXPRESULTS_BTNTOLVAL,		ATTR_DIMMED,1); //Modif MaximePAGES 03/07/2020 E205
					SetCtrlAttribute(panel,EXPRESULTS_BTNTOLPRCT,		ATTR_DIMMED,1); //Modif MaximePAGES 03/07/2020 E205
					SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_TXTUNITBODY, "frames"); //Modif MaximePAGES 03/07/2020 E206
					SetCtrlAttribute(panel,EXPRESULTS_ATTRIBUTENAME,	ATTR_DIMMED,1);
					
					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG_FIXEDVALUE,ATTR_DIMMED,0); //Modif MaximePAGES 06/07/2020 E207
					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG_SEQVALUE,	 ATTR_DIMMED,1); //Modif MaximePAGES 06/07/2020 E207
					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG_RANGEVALUE,ATTR_DIMMED,1); //Modif MaximePAGES 06/07/2020 E207

					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DECORATION_7,	ATTR_VISIBLE, 0); //Modif MaximePAGES 10/07/2020
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTINCH,		ATTR_VISIBLE, 0); //Modif MaximePAGES 10/07/2020
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_WHEELDIM,		ATTR_VISIBLE, 0); //Modif MaximePAGES 10/07/2020


					break;
				case 7: //CompareP - comparer la valeur de P
					SetCtrlAttribute(panel,EXPRESULTS_LABEL1,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_LABEL2,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_VALUE,			ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_TXTUNITLABEL,		ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_ADDVAL,			ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_DELVALUE,			ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG,			ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_TOLERENCE1,		ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_ADDTOL,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_TOLERENCE2,		ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_ADDTOL,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_ADDTOL_2,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_VALUEFC,			ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_ADDEXP,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_PARAMETERSDATA,	ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_FIELDCHECK,		ATTR_DIMMED,1);
					insert_parameter = 1;
					fieldNeeded =  0;
					SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_TXTUNITBODY, ""); //Modif MaximePAGES 03/07/2020 E206
					SetCtrlAttribute(panel,EXPRESULTS_BTNTOLVAL,		ATTR_DIMMED,0); //Modif MaximePAGES 03/07/2020 E205
					SetCtrlAttribute(panel,EXPRESULTS_BTNTOLPRCT,		ATTR_DIMMED,0); //Modif MaximePAGES 03/07/2020 E205

					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG_FIXEDVALUE,ATTR_DIMMED,1); //Modif MaximePAGES 06/07/2020 E207
					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG_SEQVALUE,	 ATTR_DIMMED,1); //Modif MaximePAGES 06/07/2020 E207
					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG_RANGEVALUE,ATTR_DIMMED,1); //Modif MaximePAGES 06/07/2020 E207
					SetCtrlAttribute(panel,EXPRESULTS_ATTRIBUTENAME,	ATTR_DIMMED,1);
					
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DECORATION_7,	ATTR_VISIBLE, 0); //Modif MaximePAGES 10/07/2020
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTINCH,		ATTR_VISIBLE, 0); //Modif MaximePAGES 10/07/2020
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_WHEELDIM,		ATTR_VISIBLE, 0); //Modif MaximePAGES 10/07/2020
				//	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA,	ATTR_DIMMED,1);   

					break;
				case 8: //CompareAcc - Comparer la valeur de Acc
					SetCtrlAttribute(panel,EXPRESULTS_LABEL1,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_LABEL2,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_VALUE,			ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_TXTUNITLABEL,		ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_ADDVAL,			ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_DELVALUE,			ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG,			ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_TOLERENCE1,		ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_ADDTOL,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_TOLERENCE2,		ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_ADDTOL,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_ADDTOL_2,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_ADDEXP,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_VALUEFC,			ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_PARAMETERSDATA,	ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_FIELDCHECK,		ATTR_DIMMED,1);
					insert_parameter = 1;
					fieldNeeded =  0;
					SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_TXTUNITBODY, ""); //Modif MaximePAGES 03/07/2020 E206
					SetCtrlAttribute(panel,EXPRESULTS_BTNTOLVAL,		ATTR_DIMMED,0); //Modif MaximePAGES 03/07/2020 E205
					SetCtrlAttribute(panel,EXPRESULTS_BTNTOLPRCT,		ATTR_DIMMED,0); //Modif MaximePAGES 03/07/2020 E205
					SetCtrlAttribute(panel,EXPRESULTS_ATTRIBUTENAME,	ATTR_DIMMED,1);
					
					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG_FIXEDVALUE,ATTR_DIMMED,1); //Modif MaximePAGES 06/07/2020 E207
					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG_SEQVALUE,	 ATTR_DIMMED,1); //Modif MaximePAGES 06/07/2020 E207
					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG_RANGEVALUE,ATTR_DIMMED,1); //Modif MaximePAGES 06/07/2020 E207

					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DECORATION_7,	ATTR_VISIBLE, 1); //Modif MaximePAGES 10/07/2020
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTINCH,		ATTR_VISIBLE, 1); //Modif MaximePAGES 10/07/2020
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_WHEELDIM,		ATTR_VISIBLE, 1); //Modif MaximePAGES 10/07/2020
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_WHEELDIM,		ATTR_DIMMED, 0);
				//	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA,	ATTR_DIMMED,1);   
					
					break;
				case 9: //CheckNoRF - verifier qu'il n'y a pas de reponse
					SetCtrlAttribute(panel,EXPRESULTS_LABEL1,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_LABEL2,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_VALUE,			ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_TXTUNITLABEL,		ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_ADDVAL,			ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_DELVALUE,			ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG,			ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_TOLERENCE1,		ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_TOLERENCE2,		ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_ADDTOL,			ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_ADDTOL_2,			ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_ADDEXP,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_FIELDCHECK,		ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_VALUEFC,			ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_PARAMETERSDATA,	ATTR_DIMMED,0);
					insert_parameter = 1;
					fieldNeeded =  0;
					SetCtrlAttribute(panel,EXPRESULTS_BTNTOLVAL,		ATTR_DIMMED,1); //Modif MaximePAGES 03/07/2020 E205
					SetCtrlAttribute(panel,EXPRESULTS_BTNTOLPRCT,		ATTR_DIMMED,1); //Modif MaximePAGES 03/07/2020 E205
					SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_TXTUNITBODY, ""); //Modif MaximePAGES 03/07/2020 E206
					SetCtrlAttribute(panel,EXPRESULTS_ATTRIBUTENAME,	ATTR_DIMMED,1);
				//	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA,	ATTR_DIMMED,1);
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DECORATION_7,	ATTR_VISIBLE, 0); //Modif MaximePAGES 10/07/2020
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTINCH,		ATTR_VISIBLE, 0); //Modif MaximePAGES 10/07/2020
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_WHEELDIM,		ATTR_VISIBLE, 0); //Modif MaximePAGES 10/07/2020


					break;
				case 10: //CheckTimingFirstRF - verifier les temps de reponse à la LF DP and Motion
					SetCtrlAttribute(panel,EXPRESULTS_VALUEFC,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_LABEL1,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_LABEL2,			ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_VALUE,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_TXTUNITLABEL,		ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_ADDVAL,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_DELVALUE,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_TOLERENCE1,		ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_TOLERENCE2,		ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_ADDTOL,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_ADDTOL_2,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_ADDEXP,			ATTR_DIMMED,0); //mettre une verification
					SetCtrlAttribute(panel,EXPRESULTS_PARAMETERSDATA,	ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_FIELDCHECK,		ATTR_DIMMED,1);
					SetCtrlAttribute(panel,EXPRESULTS_VALUEFC,			ATTR_DIMMED,0);
					SetCtrlAttribute(panel,EXPRESULTS_BTNTOLVAL,		ATTR_DIMMED,0); //Modif MaximePAGES 03/07/2020 E205
					SetCtrlAttribute(panel,EXPRESULTS_BTNTOLPRCT,		ATTR_DIMMED,0); //Modif MaximePAGES 03/07/2020 E205
					SetCtrlAttribute(panel,EXPRESULTS_ATTRIBUTENAME,	ATTR_DIMMED,1);
				//	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA,	ATTR_DIMMED,1);
					insert_parameter = 1;
					fieldNeeded =  0;
					SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_TXTUNITBODY, "[s]"); //Modif MaximePAGES 03/07/2020 E206

					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG_FIXEDVALUE,ATTR_DIMMED,1); //Modif MaximePAGES 06/07/2020 E207
					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG_SEQVALUE,	 ATTR_DIMMED,1); //Modif MaximePAGES 06/07/2020 E207
					SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG_RANGEVALUE,ATTR_DIMMED,0); //Modif MaximePAGES 06/07/2020 E207

					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DECORATION_7,	ATTR_VISIBLE, 0); //Modif MaximePAGES 10/07/2020
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTINCH,		ATTR_VISIBLE, 0); //Modif MaximePAGES 10/07/2020
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_WHEELDIM,		ATTR_VISIBLE, 0); //Modif MaximePAGES 10/07/2020


					break;

			}

	}
	return 0;
}



//***********************************************************************************************
// Modif - Carolina 02/2019
// Table ...
// Not finished
//***********************************************************************************************
int CVICALLBACK addexp (int panel, int control, int event,
						void *callbackData, int eventData1, int eventData2)
{
//	int valueBox = 0;
//	int valueBox2 = 0;
	int itemValue = 0;
	//int ListIndex = 0;
	int interfacedata = 0;
	char noItemSelected[2] = "";

	char itemSelected[255] = "";
	char itemSelected2[255] = "";


	
	//char name1[100] = "Label_";
	//char name2[100] = " | Label_";
	char name1[100] = "";
	char name2[100] = " | ";
	char *name3;
	char *finallabel;
	char labelname1[200]="";
	char labelname2[200]="";

	//Type de valeur à verifier
	char TypeValue[500] = "";
	char TypeValue2[500] = "";


	char valueFC[50] = "";
	char valueFC2[50] = "";

	char Tolerencepercen[500] = "";
	char Tolerencepercen2[500] = "";
	char Tolerencevalue[500] = "";
	char Tolerencevalue2[500] = "";

	// Nom des fonctions du menu
	//char function[]="0";
	char function1[]="CheckTimingInterFrames";
	char function2[]="CheckTimingInterBursts";
	char function3[]="CheckFieldValue";
	char function4[]="CheckSTDEV";
	char function5[]="CheckNbBursts";
	char function6[]="CheckNbFramesInBurst";
	char function7[]="CheckCompareP";
	char function8[]="CheckCompareAcc";
	char function9[]="CheckNoRF";
	char function10[]="CheckTimingFirstRF";



	Point cell; 

	switch (event)
	{
		case EVENT_COMMIT:
			//MARVYN 23/06/2021
		
	
	
			if(insertSteps == 0 && modifySteps == 0)
			{
			InsertTableRows (GiPanel, PANEL_MODE_EXP_RESULTS, -1, 1, VAL_USE_MASTER_CELL_TYPE);
			*PointeurNbRowsExpR= *PointeurNbRowsExpR+1;
			cell.y=*PointeurNbRowsExpR;
			}
			else if (insertSteps ==1)
			{
			
			
				InsertTableRows (GiPanel, PANEL_MODE_EXP_RESULTS, numRow, 1, VAL_USE_MASTER_CELL_TYPE);
				*PointeurNbRowsExpR= *PointeurNbRowsExpR+1;
				cell.y = numRow ;
			
			}
			else if (modifySteps == 1)
			{
				
			cell.y = numRow ; 
			//InsertTableRows (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, numRow, 1, VAL_USE_MASTER_CELL_TYPE);
			//DeleteTableRows (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, numRow+1, tableHeight);
			itemValue= commandExpResult  ;
			}
			
			
			if ( modifySteps == 1)
			{
			couleurSelection(numRow,PANEL_MODE_EXP_RESULTS,GiPanel,VAL_WHITE); 
			}
			else if (insertSteps == 1)
			{
			couleurSelection(numRow+1,PANEL_MODE_EXP_RESULTS,GiPanel,VAL_WHITE); 
			}
			
			//************
			DeleteListItem (GiExpectedResultsPanel, EXPRESULTS_FIELDCHECK, 0, -1);
			//DeleteListItem (GiExpectedResultsPanel, EXPRESULTS_SELECTPARAM, 0, -1);

			/*
			//visible the parameters
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_SELECTPARAM,		ATTR_VISIBLE, 0);
			//SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDFORMULEXP,	ATTR_VISIBLE, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,		ATTR_VISIBLE, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_INSERTFORMULE,	ATTR_VISIBLE, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_7,		ATTR_VISIBLE, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DECORATION_5,	ATTR_VISIBLE, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTPAR2,			ATTR_VISIBLE, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTPAR1,			ATTR_VISIBLE, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTSUM,			ATTR_VISIBLE, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTSUB,			ATTR_VISIBLE, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTMULT,			ATTR_VISIBLE, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTDIV,			ATTR_VISIBLE, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_EXPUNIT,			ATTR_VISIBLE, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_EXPVALUE,		ATTR_VISIBLE, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_EXPVALUE_2,		ATTR_VISIBLE, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_8,		ATTR_VISIBLE, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DELETEBUTTON,	ATTR_VISIBLE, 0);
			*/
			
			dimmedNumericKeypadForExp(1); 
			
			//enable the parameters
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_SELECTPARAM,		ATTR_DIMMED, 1);
			//SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDFORMULEXP,	ATTR_DIMMED, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,		ATTR_DIMMED, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_INSERTFORMULE,	ATTR_DIMMED, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_EXPVALUE_2,		ATTR_DIMMED, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_7,		ATTR_DIMMED, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DELETEBUTTON,	ATTR_DIMMED, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ATTRIBUTENAME,	ATTR_DIMMED,1);  
			
			
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,	ATTR_DIMMED, 1);



			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_LABEL1,			ATTR_DIMMED,1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_LABEL2,			ATTR_DIMMED,1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_VALUE,			ATTR_DIMMED,1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TXTUNITLABEL,	ATTR_DIMMED,1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG,			ATTR_DIMMED,1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE1,		ATTR_DIMMED,1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE2,		ATTR_DIMMED,1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDEXP,			ATTR_DIMMED,1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDTOL,			ATTR_DIMMED,1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDTOL_2,		ATTR_DIMMED,1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDVAL,			ATTR_DIMMED,1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,		ATTR_DIMMED,1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA,	ATTR_DIMMED,1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_VALUEFC,			ATTR_DIMMED,1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DELVALUE,		ATTR_DIMMED,1);
			SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_TXTUNITBODY, ""); //Modif MaximePAGES 03/07/2020 E206
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BTNTOLVAL,		ATTR_DIMMED,1); //Modif MaximePAGES 03/07/2020 E205
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BTNTOLPRCT,		ATTR_DIMMED,1); //Modif MaximePAGES 03/07/2020 E205
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_BTNTOLVAL,1);  //Modif MaximePAGES 03/07/2020 E205
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_BTNTOLPRCT,0); //Modif MaximePAGES 03/07/2020 E205

			SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG_FIXEDVALUE,ATTR_DIMMED,1); //Modif MaximePAGES 06/07/2020 E207
			SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG_SEQVALUE,	 ATTR_DIMMED,1); //Modif MaximePAGES 06/07/2020 E207
			SetCtrlAttribute(panel,EXPRESULTS_TEXTMSG_RANGEVALUE,ATTR_DIMMED,1); //Modif MaximePAGES 06/07/2020 E207


			//Modif MaximePAGES 13/08/2020- WU ID selection in IHM
			SetCtrlAttribute(panel,EXPRESULTS_DECORATION_8,			ATTR_DIMMED,1);
			SetCtrlAttribute(panel,EXPRESULTS_DECORATION_9,			ATTR_DIMMED,1);
			SetCtrlAttribute(panel,EXPRESULTS_WUIDEXP,				ATTR_DIMMED,1);
			SetCtrlAttribute(panel,EXPRESULTS_WUID_VALUE,			ATTR_DIMMED,1);

		
		
			
			if (modifySteps ==0)
			{
			GetCtrlVal(panel,EXPRESULTS_EXPLIST, &itemValue);  // troisieme funcçao
			}
			
			Valsequencevalue = 0;
			Valonevalue = 0;
			Valrangevalue = 0;

			count_pass=0;

			switch (itemValue)
			{
				case 0:
					//SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, function);
					//MessagePopup ("Warning", "");
					break;

				case 1:  //CheckTimingInterFrames
					cell.x=1;  
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, function1);

					cell.x=2;
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_VALUEFC, valueFC);
					sprintf(valueFC2,"%s",valueFC);
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, valueFC2);

					cell.x=3;  //  2 labels in the same cell
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_LABEL1, labelname1);
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_LABEL2, labelname2);

					strcat(name1, labelname1); 				// Label_label1
					strcat(name2, labelname2);    			// " | Label_label2
					name3 = strcat(name1, name2);	  		// Label_label1 | Label_label2
					finallabel =  strcat(name3, ";"); 		// Label_label1 | Label_label2;
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell,finallabel);

					cell.x=4;
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA,&interfacedata);
					switch (interfacedata)
					{
						case 1:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[0].name);
							break;
						case 2:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[1].name);
							break;
						case 3:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[2].name);
							break;
						case 4:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[3].name);
							break;
						case 5:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[4].name);
							break;
						case 6:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[5].name);
							break;
						case 7:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[6].name);
							break;
						case 8:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[7].name);
							break;
						case 9:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[8].name);
							break;
						case 10:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[9].name);
							break;
						case 11:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[10].name);
							break;
						case 12:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[11].name);
							break;
						case 13:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[12].name);
							break;
							
					}

					cell.x=5;  //
					GetCtrlVal(GiExpectedResultsPanel, EXPRESULTS_TAKEFIELD, itemSelected);
					if(strcmp(itemSelected, noItemSelected) == 0) //no itens selected
					{
						if (modifySteps == 0 && fieldNeeded == 1)
						{
						//error
						MessagePopup("Warning", "No field selected!");
						}
					}
					else
					{
						sprintf(itemSelected2,"%s",itemSelected);
						SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, itemSelected2);
					}


					cell.x=6;  //add value in cell 6
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_VALUE, TypeValue);
					sprintf(TypeValue2,"%s;",TypeValue);
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, TypeValue2);

					cell.x=7;
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE2, Tolerencevalue);
					if(strcmp(Tolerencevalue, "") == 0)
					{
						//default value
						sprintf(Tolerencevalue2,"0;");
						SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Tolerencevalue2);
					}
					else
					{
						sprintf(Tolerencevalue2,"%s;",Tolerencevalue);
						SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Tolerencevalue2);
					}

					cell.x=8;
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE1, Tolerencepercen);
					if(strcmp(Tolerencepercen, "") == 0)
					{
						sprintf(Tolerencepercen2,"0;");
						SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Tolerencepercen2);
					}
					else
					{
						sprintf(Tolerencepercen2,"%s;",Tolerencepercen);
						SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Tolerencepercen2);
					}



					break;

				case 2:		   //  CheckTimingInterBursts
					cell.x=1;  //
					//SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Libelle2);
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, function2);

					cell.x=2;
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_VALUEFC, valueFC);
					sprintf(valueFC2,"%s",valueFC);
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, valueFC2);

					cell.x=3;  //  2 labels in the same cell
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_LABEL1, labelname1);
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_LABEL2, labelname2);

					strcat(name1, labelname1); 				// Label_label1
					strcat(name2, labelname2);    			// " | Label_label2
					name3 = strcat(name1, name2);	  		// Label_label1 | Label_label2
					finallabel =  strcat(name3, ";"); 		// Label_label1 | Label_label2;
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell,finallabel);

					cell.x=4;
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA,&interfacedata);
						switch (interfacedata)
					{
						case 1:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[0].name);
							break;
						case 2:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[1].name);
							break;
						case 3:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[2].name);
							break;
						case 4:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[3].name);
							break;
						case 5:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[4].name);
							break;
						case 6:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[5].name);
							break;
						case 7:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[6].name);
							break;
						case 8:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[7].name);
							break;
						case 9:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[8].name);
							break;
						case 10:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[9].name);
							break;
						case 11:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[10].name);
							break;
						case 12:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[11].name);
							break;
						case 13:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[12].name);
							break;
					}

					cell.x=5;  //
					GetCtrlVal(GiExpectedResultsPanel, EXPRESULTS_TAKEFIELD, itemSelected);
					if(strcmp(itemSelected, noItemSelected) == 0) //no itens selected
					{
					if (modifySteps == 0)
						{
						//error
						MessagePopup("Warning", "No field selected!");
						}
					}
					else
					{
						sprintf(itemSelected2,"%s",itemSelected);
						SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, itemSelected2);
					}

					cell.x=6;  //add value in cell 4
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_VALUE, TypeValue);
					sprintf(TypeValue2,"%s;",TypeValue);
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, TypeValue2);

					cell.x=7;
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE2, Tolerencevalue);
					if(strcmp(Tolerencevalue, "") == 0)
					{
						//default value
						sprintf(Tolerencevalue2,"0;");
						SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Tolerencevalue2);
					}
					else
					{
						sprintf(Tolerencevalue2,"%s;",Tolerencevalue);
						SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Tolerencevalue2);
					}

					cell.x=8;
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE1, Tolerencepercen);
					if(strcmp(Tolerencepercen, "") == 0)
					{
						sprintf(Tolerencepercen2,"0;");
						SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Tolerencepercen2);
					}
					else
					{
						sprintf(Tolerencepercen2,"%s;",Tolerencepercen);
						SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Tolerencepercen2);
					}

					break;
				case 3:
					cell.x=1;  //  CheckFieldValue
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, function3);

					cell.x=3;  //  2 labels in the same cell
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_LABEL1, labelname1);
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_LABEL2, labelname2);

					strcat(name1, labelname1); 				// Label_label1
					strcat(name2, labelname2);    			// " | Label_label2
					name3 = strcat(name1, name2);	  		// Label_label1 | Label_label2
					finallabel =  strcat(name3, ";"); 		// Label_label1 | Label_label2;
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell,finallabel);

					cell.x=4;
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA,&interfacedata);
						switch (interfacedata)
					{
						case 1:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[0].name);
							break;
						case 2:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[1].name);
							break;
						case 3:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[2].name);
							break;
						case 4:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[3].name);
							break;
						case 5:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[4].name);
							break;
						case 6:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[5].name);
							break;
						case 7:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[6].name);
							break;
						case 8:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[7].name);
							break;
						case 9:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[8].name);
							break;
						case 10:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[9].name);
							break;
						case 11:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[10].name);
							break;
						case 12:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[11].name);
							break;
						case 13:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[12].name);
							break;
					}

					cell.x=5;  //
					GetCtrlVal(GiExpectedResultsPanel, EXPRESULTS_TAKEFIELD, itemSelected);
					if(strcmp(itemSelected, noItemSelected) == 0) //no itens selected
					{
					if (modifySteps == 0)
						{
						//error
						MessagePopup("Warning", "No field selected!");
						}
					}
					else
					{
						sprintf(itemSelected2,"%s",itemSelected);
						SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, itemSelected2);
					}

					cell.x=6;  //add value in cell 4
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_VALUE, TypeValue);
					sprintf(TypeValue2,"%s;",TypeValue);
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, TypeValue2);

					cell.x=7;
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE2, Tolerencevalue);
					if(strcmp(Tolerencevalue, "") == 0)
					{
						//default value
						sprintf(Tolerencevalue2,"0;");
						SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Tolerencevalue2);
					}
					else
					{
						sprintf(Tolerencevalue2,"%s;",Tolerencevalue);
						SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Tolerencevalue2);
					}

					cell.x=8;
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE1, Tolerencepercen);
					if(strcmp(Tolerencepercen, "") == 0)
					{
						sprintf(Tolerencepercen2,"0;");
						SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Tolerencepercen2);
					}
					else
					{
						sprintf(Tolerencepercen2,"%s;",Tolerencepercen);
						SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Tolerencepercen2);
					}

					break;
				case 4:   //   CheckSTDEV
					cell.x=1;
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, function4);

					cell.x=2;
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_VALUEFC, valueFC);
					sprintf(valueFC2,"%s",valueFC);
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, valueFC2);

					cell.x=3;  //  2 labels in the same cell
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_LABEL1, labelname1);
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_LABEL2, labelname2);

					strcat(name1, labelname1); 				// Label_label1
					strcat(name2, labelname2);    			// " | Label_label2
					name3 = strcat(name1, name2);	  		// Label_label1 | Label_label2
					finallabel =  strcat(name3, ";"); 		// Label_label1 | Label_label2;
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell,finallabel);

					cell.x=4;
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA,&interfacedata);
						switch (interfacedata)
					{
						case 1:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[0].name);
							break;
						case 2:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[1].name);
							break;
						case 3:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[2].name);
							break;
						case 4:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[3].name);
							break;
						case 5:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[4].name);
							break;
						case 6:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[5].name);
							break;
						case 7:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[6].name);
							break;
						case 8:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[7].name);
							break;
						case 9:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[8].name);
							break;
						case 10:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[9].name);
							break;
						case 11:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[10].name);
							break;
						case 12:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[11].name);
							break;
						case 13:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[12].name);
							break;
					}

					cell.x=5;  //
					GetCtrlVal(GiExpectedResultsPanel, EXPRESULTS_TAKEFIELD, itemSelected);
					if(strcmp(itemSelected, noItemSelected) == 0) //no itens selected
					{
					if (modifySteps == 0)
						{
						//error
						MessagePopup("Warning", "No field selected!");
						}
					}
					else
					{
						sprintf(itemSelected2,"%s",itemSelected);
						SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, itemSelected2);
					}

					cell.x=6;  //add value in cell 4
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_VALUE, TypeValue);
					sprintf(TypeValue2,"%s;",TypeValue);
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, TypeValue2);

					break;
				case 5:   //  CheckNbBursts
					cell.x=1;
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, function5);

					cell.x=2;
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_VALUEFC, valueFC);
					sprintf(valueFC2,"%s",valueFC);
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, valueFC2);

					cell.x=3;  //  2 labels in the same cell
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_LABEL1, labelname1);
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_LABEL2, labelname2);

					strcat(name1, labelname1); 				// Label_label1
					strcat(name2, labelname2);    			// " | Label_label2
					name3 = strcat(name1, name2);	  		// Label_label1 | Label_label2
					finallabel =  strcat(name3, ";"); 		// Label_label1 | Label_label2;
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell,finallabel);

					cell.x=4;
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA,&interfacedata);
						switch (interfacedata)
					{
						case 1:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[0].name);
							break;
						case 2:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[1].name);
							break;
						case 3:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[2].name);
							break;
						case 4:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[3].name);
							break;
						case 5:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[4].name);
							break;
						case 6:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[5].name);
							break;
						case 7:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[6].name);
							break;
						case 8:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[7].name);
							break;
						case 9:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[8].name);
							break;
						case 10:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[9].name);
							break;
						case 11:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[10].name);
							break;
						case 12:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[11].name);
							break;
						case 13:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[12].name);
							break;
					}

					cell.x=5;  //
					GetCtrlVal(GiExpectedResultsPanel, EXPRESULTS_TAKEFIELD, itemSelected);
					if(strcmp(itemSelected, noItemSelected) == 0) //no itens selected
					{
					if (modifySteps == 0)
						{
						//error
						MessagePopup("Warning", "No field selected!");
						}
					}
					else
					{
						sprintf(itemSelected2,"%s",itemSelected);
						SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, itemSelected2);
					}

					cell.x=6;  //add value in cell 4
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_VALUE, TypeValue);
					sprintf(TypeValue2,"%s;",TypeValue);
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, TypeValue2);

					break;
				case 6: // CheckNbFramesInBurst
					cell.x=1;  //
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, function6);

					cell.x=2;
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_VALUEFC, valueFC);
					sprintf(valueFC2,"%s",valueFC);
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, valueFC2);

					cell.x=3;  //  2 labels in the same cell
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_LABEL1, labelname1);
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_LABEL2, labelname2);

					strcat(name1, labelname1); 				// Label_label1
					strcat(name2, labelname2);    			// " | Label_label2
					name3 = strcat(name1, name2);	  		// Label_label1 | Label_label2
					finallabel =  strcat(name3, ";"); 		// Label_label1 | Label_label2;
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell,finallabel);

					cell.x=4;
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA,&interfacedata);
					switch (interfacedata)
					{
						case 1:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[0].name);
							break;
						case 2:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[1].name);
							break;
						case 3:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[2].name);
							break;
						case 4:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[3].name);
							break;
						case 5:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[4].name);
							break;
						case 6:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[5].name);
							break;
						case 7:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[6].name);
							break;
						case 8:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[7].name);
							break;
						case 9:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[8].name);
							break;
						case 10:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[9].name);
							break;
						case 11:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[10].name);
							break;
						case 12:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[11].name);
							break;
						case 13:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[12].name);
							break;
					}

					cell.x=5;  //
					GetCtrlVal(GiExpectedResultsPanel, EXPRESULTS_TAKEFIELD, itemSelected);
					if(strcmp(itemSelected, noItemSelected) == 0) //no itens selected
					{
					if (modifySteps == 0)
						{
						//error
						MessagePopup("Warning", "No field selected!");
						}
					}
					else
					{
						sprintf(itemSelected2,"%s",itemSelected);
						SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, itemSelected2);
					}

					cell.x=6;  //add value in cell 4
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_VALUE, TypeValue);
					sprintf(TypeValue2,"%s;",TypeValue);
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, TypeValue2);

					break;
				case 7:   // CheckCompareP
					cell.x=1;  //
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, function7);

					cell.x=2;
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_VALUEFC, valueFC);
					sprintf(valueFC2,"%s",valueFC);
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, valueFC2);

					cell.x=3;  //  2 labels in the same cell
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_LABEL1, labelname1);
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_LABEL2, labelname2);

					strcat(name1, labelname1); 				// Label_label1
					strcat(name2, labelname2);    			// " | Label_label2
					name3 = strcat(name1, name2);	  		// Label_label1 | Label_label2
					finallabel =  strcat(name3, ";"); 		// Label_label1 | Label_label2;
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell,finallabel);

					
					cell.x=4;
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA,&interfacedata);
					switch (interfacedata)
					{
						case 1:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[0].name);
							break;
						case 2:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[1].name);
							break;
						case 3:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[2].name);
							break;
						case 4:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[3].name);
							break;
						case 5:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[4].name);
							break;
						case 6:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[5].name);
							break;
						case 7:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[6].name);
							break;
						case 8:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[7].name);
							break;
						case 9:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[8].name);
							break;
						case 10:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[9].name);
							break;
						case 11:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[10].name);
							break;
						case 12:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[11].name);
							break;
						case 13:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[12].name);
							break;
					}

					cell.x=5;  //
					GetCtrlVal(GiExpectedResultsPanel, EXPRESULTS_TAKEFIELD, itemSelected);
					if(strcmp(itemSelected, noItemSelected) == 0) //no itens selected
					{
					if (modifySteps == 0)
						{
						//error
						MessagePopup("Warning", "No field selected!");
						}
					}
					else
					{
						sprintf(itemSelected2,"%s",itemSelected);
						SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, itemSelected2);
					}

					
					
					cell.x=7;
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE2, Tolerencevalue);
					if(strcmp(Tolerencevalue, "") == 0)
					{
						//default value
						sprintf(Tolerencevalue2,"0;");
						SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Tolerencevalue2);
					}
					else
					{
						sprintf(Tolerencevalue2,"%s;",Tolerencevalue);
						SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Tolerencevalue2);
					}

					cell.x=8;
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE1, Tolerencepercen);
					if(strcmp(Tolerencepercen, "") == 0)
					{
						sprintf(Tolerencepercen2,"0;");
						SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Tolerencepercen2);
					}
					else
					{
						sprintf(Tolerencepercen2,"%s;",Tolerencepercen);
						SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Tolerencepercen2);
					}



					break;
				case 8:   // CheckCompareAcc

					//Wheel Dim in value field
					cell.x=6;  //add value in cell 4
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_WHEELDIM, TypeValue);
					if (strcmp(TypeValue, "") == 0)
					{
						//error
						MessagePopup("Warning", "Please enter the RIM Diameter.");
						break;
					}
					sprintf(TypeValue2,"%s;",TypeValue);
					//wheeldim = (double)TypeValue;
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, TypeValue2);

					cell.x=1;  //
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, function8);

					cell.x=2;
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_VALUEFC, valueFC);
					sprintf(valueFC2,"%s",valueFC);
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, valueFC2);

					cell.x=3;  //  2 labels in the same cell
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_LABEL1, labelname1);
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_LABEL2, labelname2);

					strcat(name1, labelname1); 				// Label_label1
					strcat(name2, labelname2);    			// " | Label_label2
					name3 = strcat(name1, name2);	  		// Label_label1 | Label_label2
					finallabel =  strcat(name3, ";"); 		// Label_label1 | Label_label2;
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell,finallabel);

					cell.x=4;
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA,&interfacedata);
					switch (interfacedata)
					{
						case 1:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[0].name);
							break;
						case 2:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[1].name);
							break;
						case 3:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[2].name);
							break;
						case 4:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[3].name);
							break;
						case 5:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[4].name);
							break;
						case 6:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[5].name);
							break;
						case 7:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[6].name);
							break;
						case 8:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[7].name);
							break;
						case 9:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[8].name);
							break;
						case 10:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[9].name);
							break;
						case 11:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[10].name);
							break;
						case 12:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[11].name);
							break;
						case 13:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[12].name);
							break;
					}

					cell.x=5;  //
					GetCtrlVal(GiExpectedResultsPanel, EXPRESULTS_TAKEFIELD, itemSelected);
					if(strcmp(itemSelected, noItemSelected) == 0) //no itens selected
					{
						if (modifySteps == 0)
						{
						//error
						MessagePopup("Warning", "No field selected!");
						}
					}
					else
					{
						sprintf(itemSelected2,"%s",itemSelected);
						SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, itemSelected2);
					}



					cell.x=7;
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE2, Tolerencevalue);
					if(strcmp(Tolerencevalue, "") == 0)
					{
						//default value
						sprintf(Tolerencevalue2,"0;");
						SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Tolerencevalue2);
					}
					else
					{
						sprintf(Tolerencevalue2,"%s;",Tolerencevalue);
						SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Tolerencevalue2);
					}

					cell.x=8;
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE1, Tolerencepercen);
					if(strcmp(Tolerencepercen, "") == 0)
					{
						sprintf(Tolerencepercen2,"0;");
						SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Tolerencepercen2);
					}
					else
					{
						sprintf(Tolerencepercen2,"%s;",Tolerencepercen);
						SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Tolerencepercen2);
					}

					break;
				case 9:   // CheckNoRF
					cell.x=1;  //
					//SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Libelle9);
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, function9);

					cell.x=3;  //  2 labels in the same cell
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_LABEL1, labelname1);
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_LABEL2, labelname2);

					strcat(name1, labelname1); 				// Label_label1
					strcat(name2, labelname2);    			// " | Label_label2
					name3 = strcat(name1, name2);	  		// Label_label1 | Label_label2
					finallabel =  strcat(name3, ";"); 		// Label_label1 | Label_label2;
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell,finallabel);

					cell.x=4;
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA,&interfacedata);
						switch (interfacedata)
					{
						case 1:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[0].name);
							break;
						case 2:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[1].name);
							break;
						case 3:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[2].name);
							break;
						case 4:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[3].name);
							break;
						case 5:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[4].name);
							break;
						case 6:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[5].name);
							break;
						case 7:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[6].name);
							break;
						case 8:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[7].name);
							break;
						case 9:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[8].name);
							break;
						case 10:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[9].name);
							break;
						case 11:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[10].name);
							break;
						case 12:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[11].name);
							break;
						case 13:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[12].name);
							break;
					}

					cell.x=5;  //
					GetCtrlVal(GiExpectedResultsPanel, EXPRESULTS_TAKEFIELD, itemSelected);
					if(strcmp(itemSelected, noItemSelected) == 0) //no itens selected
					{
					if (modifySteps == 0)
						{
						//error
						MessagePopup("Warning", "No field selected!");
						}
					}
					else
					{
						sprintf(itemSelected2,"%s",itemSelected);
						SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, itemSelected2);
					}

					
					
					
					break;
				case 10:   // CheckTimingFirstRF
					FirstRF = 1;
					cell.x=1;  //
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, function10);

					cell.x=2;
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_VALUEFC, valueFC);
					sprintf(valueFC2,"%s",valueFC);
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, valueFC2);

					cell.x=3;  //  2 labels in the same cell
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_LABEL1, labelname1);
					finallabel = strcat(name1, labelname1); //	// Label_label1
					finallabel =  strcat(name1, ";");
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, finallabel);

					cell.x=4;
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA,&interfacedata);
						switch (interfacedata)
					{
						case 1:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[0].name);
							break;
						case 2:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[1].name);
							break;
						case 3:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[2].name);
							break;
						case 4:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[3].name);
							break;
						case 5:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[4].name);
							break;
						case 6:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[5].name);
							break;
						case 7:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[6].name);
							break;
						case 8:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[7].name);
							break;
						case 9:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[8].name);
							break;
						case 10:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[9].name);
							break;
						case 11:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[10].name);
							break;
						case 12:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[11].name);
							break;
						case 13:
							SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Interfaces[12].name);
							break;
					}

					cell.x=5;  //
					GetCtrlVal(GiExpectedResultsPanel, EXPRESULTS_TAKEFIELD, itemSelected);
					if(strcmp(itemSelected, noItemSelected) == 0) //no itens selected
					{
					if (modifySteps == 0)
						{
						//error
						MessagePopup("Warning", "No field selected!");
						}
					}
					else
					{
						sprintf(itemSelected2,"%s",itemSelected);
						SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, itemSelected2);
					}

					cell.x=6;  //add value in cell 4
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_VALUE, TypeValue);
					sprintf(TypeValue2,"%s;",TypeValue);
					SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, TypeValue2);

					cell.x=7;
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE2, Tolerencevalue);
					if(strcmp(Tolerencevalue, "") == 0)
					{
						//default value
						sprintf(Tolerencevalue2,"0;");
						SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Tolerencevalue2);
					}
					else
					{
						sprintf(Tolerencevalue2,"%s;",Tolerencevalue);
						SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Tolerencevalue2);
					}

					cell.x=8;
					GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE1, Tolerencepercen);
					if(strcmp(Tolerencepercen, "") == 0)
					{
						sprintf(Tolerencepercen2,"0;");
						SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Tolerencepercen2);
					}
					else
					{
						sprintf(Tolerencepercen2,"%s;",Tolerencepercen);
						SetTableCellVal (GiPanel, PANEL_MODE_EXP_RESULTS, cell, Tolerencepercen2);
					}

					break;

			}
			
		
			//MARVYN 29/06/2021
			//printf("lab1 = %s and lab2 = %s\n",labelname1,labelname2);
			if (verifLabel(labelname1)==0 && strcmp(labelname1,"")!=0)
			{
			MessagePopup ("Warning", "Label1 does not exist");
			}
			if (verifLabel(labelname2)==0 && FirstRF==0 && strcmp(labelname2,"")!=0)
			{
			MessagePopup ("Warning", "Label2 does not exist");
			}
			

			//remise a zero des indicateurs
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_LABEL1,		"");
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_LABEL2,		"");
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_VALUEFC,			""); 
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_VALUE,			"");
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE1,	"");
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE2,	"");
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,	0);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA,0);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_ATTRIBUTENAME,	"");
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_SELECTPARAM,	0);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_EXPLIST,		0);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_EXPUNIT,		"");
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_EXPVALUE,		"");
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,	"");

			break;   //EVENT_COMMIT:
	}
	return 0;
}


//MARVYN 30/06/2021
//check if the Label exist to be sure to have something coherent 
int verifLabel(char *label)
{
	Point cell;
	//char *tab[100] ;
	char sCellValue[200]="";
	int find = 0;
	int iNumberOfRows ; 
	int iErr;
	
	iErr = GetNumTableRows (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, &iNumberOfRows);
	cell.x = 3 ;
	cell.y=1;
	
    for (int i=0;i<iNumberOfRows;i++)
	{
	GetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
	//printf("Label = %s\n",sCellValue);
	if (strcmp(sCellValue,"SetA")!=0 && strcmp(sCellValue,"SetP")!=0 && strcmp(sCellValue,"SendLFD")!=0 && strcmp(sCellValue,"StopLFD")!=0 && strcmp(sCellValue,"SendLFCw")!=0 && strcmp(sCellValue,"StopLFCw")!=0 && strcmp(sCellValue,"LFPower")!=0 && strcmp(sCellValue,"Label")!=0 && strcmp(sCellValue,label) == 0 )
	{
	find = 1 ;	
	}
	cell.y++;
	}
	return find;

}


							
//***********************************************************************************************
// Modif - Carolina 03/2019
//
//***********************************************************************************************
int CVICALLBACK OkCallback (int panel, int control, int event,
							void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{
		case EVENT_COMMIT:
			//printf("modif = %d and insert = %d\n",modifySteps,insertSteps);
			//Marvyn 23/07/2021 for color of lines 
			if ( modifySteps == 1 || insertSteps == 1)
			{
			couleurSelection(numRow,PANEL_MODE_EXP_RESULTS,GiPanel,VAL_WHITE);           
			}
			HidePanel(GiExpectedResultsPanel);

			DeleteListItem (GiExpectedResultsPanel, EXPRESULTS_FIELDCHECK, 0, -1);
			//DeleteListItem (GiExpectedResultsPanel, EXPRESULTS_SELECTPARAM, 0, -1);


			//ExcelRpt_WorkbookClose (workbookHandledata, 0);
			//ExcelRpt_ApplicationQuit (applicationHandledata);
			//CA_DiscardObjHandle(applicationHandledata);

			// fermeture d'excel
			ExcelRpt_WorkbookClose (workbookHandle6, 0);
			//ExcelRpt_ApplicationQuit (applicationHandle6);
			//CA_DiscardObjHandle(applicationHandle6);

			add_val = 0;
			add_tol = 0;
			add_tolpercent = 0;

			break;
	}
	return 0;
}


//***********************************************************************************************
// Modif - Carolina 03/2019
// Calculate de time
//***********************************************************************************************
int findTime(int line)
{
	Point cell;
	BOOL bParam = FALSE;
	int a=0;
	const char s[2] = " ";
	char *token;
	char *tabtoken[200] = {NULL};
	char range[30];
	char ParameterName[100];
	char unitParameter[10]="";
	char Value[100];
	char *TabValue[100] = {NULL};
	char defaultDirectory[MAX_PATHNAME_LEN] = "D:\\CalculationFile.xlsx";
	int boucle;
	int end;
	int StringEqual=0;
	char stime[50]="";
	int findend = 0;
	int stringFound = 0;
	char milisec[5] = "[ms]";
	float fValue1mili = 0;
	int iValue1=0;

	//ExcelRpt_ApplicationNew(VFALSE,&applicationHandle);
	//ExcelRpt_WorkbookOpen (applicationHandleProject, GsCheckCfgFile, &workbookHandledata); // GsCheckCfgFile ou "C:\\Users\\uid42961\\Desktop\\WU TestBench\\Automation SYP.xlsx"
	//ExcelRpt_GetWorksheetFromIndex(workbookHandledata, 2, &worksheetHandledata);


	cell.y =line;
	cell.x =1;
	sprintf(range,"A%d",line+1); //take value from table
	GetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS,cell, stime);

	strncpy (ChaineExcel, "=", 200);
	token = strtok(stime, s);
	tabtoken[a]=token;
	end=1;

	for(boucle=1; end==1; boucle++) //take from database
	{
		sprintf(range,"A%d",boucle+1);
		ExcelRpt_GetCellValue (worksheetHandledata, range, CAVT_CSTRING, ParameterName);
		StringEqual=strcmp(ParameterName, token); //erreur si time and duration sont vide

		if(StringEqual == 0)
		{
			sprintf(range,"B%d",boucle+1);
			ExcelRpt_GetCellValue (worksheetHandledata, range, CAVT_CSTRING, Value);
			sprintf(range,"C%d",boucle+1); //colonne C unités
			ExcelRpt_GetCellValue (worksheetHandledata, range, CAVT_CSTRING, unitParameter);
			TabValue[a]=Value;
			strcat(ChaineExcel, TabValue[a]);
			bParam = TRUE;
			stringFound = 1;
		}

		// cherche la fin de la liste
		/*parameterCompare=strchr(ParameterName,'end');
		if(parameterCompare != NULL)
		{
			end=0;
			findend =1;
		}*/
		if(strcmp(ParameterName, "") == 0)
		{
			end=0;
			findend =1;
		}
		if(findend ==1 && stringFound == 0)
		{
			cell.y =line;
			cell.x =1;
			sprintf(range,"A%d",line+1); //take value from table
			GetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS,cell, stime);
			//ExcelRpt_WorkbookClose (workbookHandle, 0);
			//CA_DiscardObjHandle(applicationHandle);
			//ExcelRpt_ApplicationQuit (applicationHandle);
			return atoi(stime);
		}
	}
	if (bParam==FALSE)
	{
		strcat(ChaineExcel, tabtoken[a]);
	}
	a++;

	/* walk through other tokens */
	while( token != NULL )
	{
		token = strtok(NULL, s);
		tabtoken[a]=token;
		end=1;
		bParam = FALSE;

		for(boucle=1; end==1; boucle++)
		{
			sprintf(range,"A%d",boucle+1);
			ExcelRpt_GetCellValue (worksheetHandledata, range, CAVT_CSTRING, ParameterName);

			if(token!=NULL)
			{
				StringEqual=strcmp(ParameterName, token);
			}

			if(StringEqual == 0)
			{
				sprintf(range,"B%d",boucle+1);
				ExcelRpt_GetCellValue (worksheetHandledata, range, CAVT_CSTRING, Value);

				TabValue[a]=Value;
				strcat(ChaineExcel, TabValue[a]);
				bParam = TRUE;
			}

			//parameterCompare=strchr(ParameterName,'end'); //if there is an "end" in the end of the list
			/*if(parameterCompare != NULL)
			{
				end=0;
			}  */
			if(strcmp(ParameterName, "") == 0)
			{
				end=0;
			}
		}
		if ((bParam==FALSE)&&(token!=NULL) )
		{
			strcat(ChaineExcel, tabtoken[a]);
		}
		//	if( token!=NULL)
		//	{}
		a++;
	}

	//ExcelRpt_ApplicationNew(VFALSE,&applicationHandle8);
	ExcelRpt_WorkbookOpen (applicationHandleProject,defaultDirectory, &workbookHandle8);
	ExcelRpt_GetWorksheetFromIndex(workbookHandle8, 1, &worksheetHandle8);
	ExcelRpt_SetCellValue (worksheetHandle8, "A1", CAVT_CSTRING, ChaineExcel);
	ExcelRpt_GetCellValue (worksheetHandle8, "A1", CAVT_INT, &iValue1);
	Value1 = iValue1;

	ExcelRpt_WorkbookClose (workbookHandle8, 0);
	//ExcelRpt_ApplicationQuit (applicationHandle8);
	//CA_DiscardObjHandle(applicationHandle8);
	//converts the Value on seconds
	if((strcmp(unitParameter, milisec)) == 0)
	{
		fValue1mili = (float)Value1/1000;
		return fValue1mili;
	}
	else
	{
		return Value1;
	}

}


//***********************************************************************************************
// Modif - Carolina 03/2019
//
//***********************************************************************************************
int findDuration(int line)
{
	Point cell;
	int a=0;;
	const char s[2] = " ";
	char *token;
	char *tabtoken[200] = {NULL};
	char range[30];
	char ParameterName[100];
	char Value[100];
	char unitParameter[10]="";
	char *TabValue[100] = {NULL};
	char defaultDirectory[MAX_PATHNAME_LEN] = "D:\\CalculationFile.xlsx";
	int boucle = 0;
	int end = 0;
	int StringEqual=0;
	BOOL bParam = FALSE;
	char sduration[50]="";
	int findend = 0;
	int stringFound = 0;
	char milisec[5] = "[ms]";
	float fValue2mili = 0;

	//ExcelRpt_ApplicationNew(VFALSE,&applicationHandle);
	//ExcelRpt_WorkbookOpen (applicationHandleProject, GsCheckCfgFile, &workbookHandledata); // GsCheckCfgFile ou "C:\\Users\\uid42961\\Desktop\\WU TestBench\\Automation SYP.xlsx"
	//ExcelRpt_GetWorksheetFromIndex(workbookHandledata, 2, &worksheetHandledata);

	cell.y =line;
	cell.x =2;
	sprintf(range,"B%d",line+1); //take value from table
	GetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS,cell, sduration);

	strncpy (ChaineExcel, "=", 200);
	token = strtok(sduration, s);
	tabtoken[a]=token;
	end=1;

	if(token == NULL)
	{
		cell.y =line;
		cell.x =2;
		sprintf(range,"B%d",line+1); //take value from table
		GetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS,cell, s);
		//ExcelRpt_WorkbookClose (workbookHandle, 0);
		//CA_DiscardObjHandle(applicationHandle);
		//ExcelRpt_ApplicationQuit (applicationHandle);
		return 0;
	}

	for(boucle=1; end==1; boucle++) //compare from database
	{
		sprintf(range,"A%d",boucle+1);
		ExcelRpt_GetCellValue (worksheetHandledata, range, CAVT_CSTRING, ParameterName);
		StringEqual=strcmp(ParameterName, token);

		if(StringEqual == 0) //when he finds the parameter
		{
			sprintf(range,"B%d",boucle+1);
			ExcelRpt_GetCellValue (worksheetHandledata, range, CAVT_CSTRING, Value);
			sprintf(range,"C%d",boucle+1); //colonne C unités
			ExcelRpt_GetCellValue (worksheetHandledata, range, CAVT_CSTRING, unitParameter);
			TabValue[a]=Value;
			strcat(ChaineExcel, TabValue[a]);
			bParam = TRUE;
			stringFound = 1;

		}
		// cherche la fin de la liste  -- parameterCompare always NULL until find "end"
		//parameterCompare=strchr(ParameterName,'end');
		if(strcmp(ParameterName, "") == 0)
		{
			end=0;
			findend = 1;
		}

		if(findend == 1 && stringFound == 0)
		{
			cell.y =line;
			cell.x =2;
			sprintf(range,"B%d",line+1); //take value from table
			GetTableCellVal(GiPanel,PANEL_MODE_SCRIPTS_DETAILS,cell, sduration);

			return atoi(sduration);
		}
	}

	if (bParam==FALSE)
	{
		strcat(ChaineExcel, tabtoken[a]);
	}
	a++;

	/* walk through other tokens */
	while( token != NULL )
	{
		token = strtok(NULL, s);
		tabtoken[a]=token;
		end=1;
		bParam = FALSE;

		for(boucle=1; end==1; boucle++)
		{
			sprintf(range,"A%d",boucle+1);
			ExcelRpt_GetCellValue (worksheetHandledata, range, CAVT_CSTRING, ParameterName);

			if(token!=NULL)
			{
				StringEqual=strcmp(ParameterName, token);
			}
			if(StringEqual == 0)
			{
				sprintf(range,"B%d",boucle+1);
				ExcelRpt_GetCellValue (worksheetHandledata, range, CAVT_CSTRING, Value);

				TabValue[a]=Value;
				strcat(ChaineExcel, TabValue[a]);
				bParam = TRUE;
			}

			//parameterCompare=strchr(ParameterName,'end');

			if(strcmp(ParameterName, "") == 0)
			{
				end=0;
			}
		}

		if ((bParam==FALSE)&&(token!=NULL) )
		{
			strcat(ChaineExcel, tabtoken[a]);
		}
		//	if( token!=NULL)
		//	{ }
		a++;
	}
	// ouverture d'excel
	//ExcelRpt_ApplicationNew(VFALSE,&applicationHandle8);
	ExcelRpt_WorkbookOpen (applicationHandleProject,defaultDirectory, &workbookHandle8);
	ExcelRpt_GetWorksheetFromIndex(workbookHandle8, 1, &worksheetHandle8);
	ExcelRpt_SetCellValue (worksheetHandle8, "A1", CAVT_CSTRING, ChaineExcel);
	ExcelRpt_GetCellValue (worksheetHandle8, "A1", CAVT_INT, &Value2);


	//converts the Value on seconds
	if((strcmp(unitParameter, milisec)) == 0)
	{
		fValue2mili = (float)Value2/1000;
		return fValue2mili;
	}
	else
	{
		return Value2;
	}


}




//***********************************************************************************************
// Modif - Carolina 03/2019
// The button add formule
//***********************************************************************************************
int CVICALLBACK addtol (int panel, int control, int event,
						void *callbackData, int eventData1, int eventData2)
{





	switch (event)
	{
		case EVENT_COMMIT:

			add_val = 0;
			add_tolpercent = 1;
			add_tol = 0;

			//DeleteListItem (GiExpectedResultsPanel, EXPRESULTS_SELECTPARAM, 0, -1);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE2,		"0");
			dimmedNumericKeypadForExp(0); //Modif MaximePAGES 30/07/2020 - New IHM Numeric Keypad
			/*
			//visible the parameters
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_SELECTPARAM,		ATTR_VISIBLE, 1);
			//SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDFORMULEXP,	ATTR_VISIBLE, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,		ATTR_VISIBLE, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_INSERTFORMULE,	ATTR_VISIBLE, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_7,		ATTR_VISIBLE, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DECORATION_5,	ATTR_VISIBLE, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTPAR2,			ATTR_VISIBLE, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTPAR1,			ATTR_VISIBLE, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTSUM,			ATTR_VISIBLE, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTSUB,			ATTR_VISIBLE, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTMULT,			ATTR_VISIBLE, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTDIV,			ATTR_VISIBLE, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_EXPUNIT,			ATTR_VISIBLE, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_EXPVALUE,		ATTR_VISIBLE, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_EXPVALUE_2,		ATTR_VISIBLE, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_8,		ATTR_VISIBLE, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DELETEBUTTON,	ATTR_VISIBLE, 1);

			//enable the parameters
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_SELECTPARAM,		ATTR_DIMMED, 0);
			//SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDFORMULEXP,	ATTR_DIMMED, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,		ATTR_DIMMED, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_INSERTFORMULE,	ATTR_DIMMED, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_EXPVALUE_2,		ATTR_DIMMED, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_7,		ATTR_DIMMED, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DELETEBUTTON,	ATTR_DIMMED, 0);
			*/


			//InsertListItem (GiExpectedResultsPanel,EXPRESULTS_SELECTPARAM,0,"",0);

			//ExcelRpt_WorkbookOpen (applicationHandleProject, GsCheckCfgFile, &workbookHandledata); // GsCheckCfgFile ou "C:\\Users\\uid42961\\Desktop\\WU TestBench\\Automation SYP.xlsx"
			//ExcelRpt_GetWorksheetFromIndex(workbookHandledata, 2, &worksheetHandledata2);

			// La boucle récupère les parametres du fichier excel (dynamique)
			//prend les noms des parameters (colonne A) dans le data base !
			/*for(boucle=1; end==1; boucle++)
			{
				//load parameters with % unit
				sprintf(range,"A%d",boucle+1);;
				ExcelRpt_GetCellValue (worksheetHandledata2, range, CAVT_CSTRING, ParameterName);
				//parameterCompare=strcmp(ParameterName,cond_end);	// compare les deux chaine de caractère

				if((strcmp(ParameterName,cond_end)) == 0)
				{
					end=0;
				}
				else
				{
					InsertListItem (GiExpectedResultsPanel,EXPRESULTS_SELECTPARAM,boucle,ParameterName,boucle);
				}
			}
			end=1;*/

			break;

	}

	return 0;
}

//***********************************************************************************************
// Modif - Carolina 03/2019
// The button add formule
//***********************************************************************************************
int CVICALLBACK addtol_2 (int panel, int control, int event,
						  void *callbackData, int eventData1, int eventData2)
{

	



	switch (event)
	{
		case EVENT_COMMIT:

			//DeleteListItem (GiExpectedResultsPanel, EXPRESULTS_SELECTPARAM, 0, -1);

			add_val = 0;
			add_tolpercent = 0;
			add_tol = 1;

			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE1,		"0");

			dimmedNumericKeypadForExp(0); //Modif MaximePAGES 30/07/2020 - New IHM Numeric Keypad

			/*
			//visible the parameters
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_SELECTPARAM,		ATTR_VISIBLE, 1);
			//SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDFORMULEXP,	ATTR_VISIBLE, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,		ATTR_VISIBLE, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_INSERTFORMULE,	ATTR_VISIBLE, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_7,		ATTR_VISIBLE, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DECORATION_5,	ATTR_VISIBLE, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTPAR2,			ATTR_VISIBLE, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTPAR1,			ATTR_VISIBLE, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTSUM,			ATTR_VISIBLE, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTSUB,			ATTR_VISIBLE, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTMULT,			ATTR_VISIBLE, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTDIV,			ATTR_VISIBLE, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_EXPUNIT,			ATTR_VISIBLE, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_EXPVALUE,		ATTR_VISIBLE, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_EXPVALUE_2,		ATTR_VISIBLE, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_8,		ATTR_VISIBLE, 1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DELETEBUTTON,	ATTR_VISIBLE, 1);

			//enable the parameters
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_SELECTPARAM,		ATTR_DIMMED, 0);
			//SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ADDFORMULEXP,	ATTR_DIMMED, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,		ATTR_DIMMED, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_INSERTFORMULE,	ATTR_DIMMED, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_EXPVALUE_2,		ATTR_DIMMED, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_7,		ATTR_DIMMED, 0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DELETEBUTTON,	ATTR_DIMMED, 0);
			*/
			//InsertListItem (GiExpectedResultsPanel,EXPRESULTS_SELECTPARAM,0,"",0);

			//ExcelRpt_WorkbookOpen (applicationHandleProject, GsCheckCfgFile, &workbookHandledata); // GsCheckCfgFile ou "C:\\Users\\uid42961\\Desktop\\WU TestBench\\Automation SYP.xlsx"
			//ExcelRpt_GetWorksheetFromIndex(workbookHandledata, 2, &worksheetHandledata2);

			// La boucle récupère les parametres du fichier excel (dynamique)
			//prend les noms des parameters (colonne A) dans le data base !
			/*for(boucle=1; end==1; boucle++)
			{
				//load parameters with % unit
				sprintf(range,"A%d",boucle+1);;
				ExcelRpt_GetCellValue (worksheetHandledata2, range, CAVT_CSTRING, ParameterName);
				//parameterCompare=strcmp(ParameterName,cond_end);	// compare les deux chaine de caractère

				if((strcmp(ParameterName,cond_end)) == 0)
				{
					end=0;
				}
				else
				{
					InsertListItem (GiExpectedResultsPanel,EXPRESULTS_SELECTPARAM,boucle,ParameterName,boucle);
				}
			}
			end=1;*/

			break;
	}

	return 0;
}

//***********************************************************************************************
// Modif - Carolina 03/2019
// The button add formule
//***********************************************************************************************
int CVICALLBACK addval (int panel, int control, int event,
						void *callbackData, int eventData1, int eventData2)
{






	int itemValue =0;


	switch (event)
	{
		case EVENT_COMMIT:
			//DeleteListItem (GiExpectedResultsPanel, EXPRESULTS_SELECTPARAM, 0, -1);

			if ((GiPanelValue = LoadPanelEx (0,"IhmModes.uir",PANELVALUE, __CVIUserHInst)) < 0)
			{
				return -1;
			}
			InstallPopup (GiPanelValue);

			add_val = 1;
			add_tolpercent = 0;
			add_tol = 0;
			count_pass=0;

			GetCtrlVal(panel,EXPRESULTS_EXPLIST, &itemValue);
			
			//Modif MaximePAGES 06/08/2020 ***************************
			if (itemValue == 2 || itemValue == 5 || itemValue == 6) {
				
				SetCtrlAttribute(GiPanelValue, PANELVALUE_ONEVALUE, ATTR_DIMMED,			0); 
				SetCtrlAttribute(GiPanelValue, PANELVALUE_SEQUENCEVALUE, ATTR_DIMMED,		1);
				SetCtrlAttribute(GiPanelValue, PANELVALUE_RANGEVALUE, 	ATTR_DIMMED,		1);
			}
			
			if (itemValue == 4 || itemValue == 10) {
				
				SetCtrlAttribute(GiPanelValue, PANELVALUE_ONEVALUE, ATTR_DIMMED,			1); 
				SetCtrlAttribute(GiPanelValue, PANELVALUE_SEQUENCEVALUE, ATTR_DIMMED,		1);
				SetCtrlAttribute(GiPanelValue, PANELVALUE_RANGEVALUE, 	ATTR_DIMMED,		0);
			}
			
			if (itemValue == 1) {
				
				SetCtrlAttribute(GiPanelValue, PANELVALUE_ONEVALUE, ATTR_DIMMED,			0); 
				SetCtrlAttribute(GiPanelValue, PANELVALUE_SEQUENCEVALUE, ATTR_DIMMED,		0);
				SetCtrlAttribute(GiPanelValue, PANELVALUE_RANGEVALUE, 	ATTR_DIMMED,		1);
			}
			
			
			
			
			// *******************************************************
			
			
			//InsertListItem (GiExpectedResultsPanel,EXPRESULTS_SELECTPARAM,0,"",0);

			//ExcelRpt_WorkbookOpen (applicationHandleProject, GsCheckCfgFile, &workbookHandledata); // GsCheckCfgFile ou "C:\\Users\\uid42961\\Desktop\\WU TestBench\\Automation SYP.xlsx"
			//ExcelRpt_GetWorksheetFromIndex(workbookHandledata, 2, &worksheetHandledata2);

			// La boucle récupère les parametres du fichier excel (dynamique)
			//prend les noms des parameters (colonne A) dans le data base !
			
			/*for(boucle=1; end==1; boucle++)
			{
				//load parameters with % unit
				sprintf(range,"A%d",boucle+1);;
				ExcelRpt_GetCellValue (worksheetHandledata2, range, CAVT_CSTRING, ParameterName);
				//parameterCompare=strcmp(ParameterName,cond_end);	// compare les deux chaine de caractère

				if((strcmp(ParameterName,cond_end)) == 0)
				{
					end=0;
				}
				else
				{
					InsertListItem (GiExpectedResultsPanel,EXPRESULTS_SELECTPARAM,boucle,ParameterName,boucle);
				}
			}
			end=1;
		   */

			break;
	}
	return 0;
}


//***********************************************************************************************
// Modif - Carolina 03/2019
// The button ...
//***********************************************************************************************
int CVICALLBACK insertformule (int panel, int control, int event,
							   void *callbackData, int eventData1, int eventData2)
{



	
	
	


	
	
;
	


	//int parameterCompare2;
	char Formule2[500]="";
	char Formule2withoutVariable[500]="";
	char variable[500] = "";

	switch (event)
	{
		case EVENT_COMMIT:

			DeleteListItem (GiExpectedResultsPanel, EXPRESULTS_FIELDCHECK, 0, -1);

			SetCtrlAttribute (GiExpectedResultsPanel, EXPRESULTS_SELECTPARAM, ATTR_DFLT_VALUE, 0);
			DefaultCtrl (GiExpectedResultsPanel, EXPRESULTS_SELECTPARAM);


			GetCtrlVal(GiExpectedResultsPanel,	EXPRESULTS_FORMUEXPEC,	Formule2); //prendre la valeur de la formule

			strcpy(Formule2withoutVariable,Formule2);
			
			//Modif MaximePAGES 10/08/2020 **************************************
			switch(currentTxtBox)
			{
				case 11: //Value text box
					if(Valsequencevalue == 1)
					{
						count_pass++;
						
						GetCtrlVal(GiExpectedResultsPanel,	EXPRESULTS_FORMUEXPEC,	Formule2); //insert
						//strtok(Formule2, ""); //no spaces
						GetCtrlVal(GiExpectedResultsPanel,	EXPRESULTS_VALUE,	variable);
						if (count_pass > 1)
						{
							strcat(variable, ";");		
						}
						//strcat(Formule2, ";");
						strcat(variable,Formule2);
						SetCtrlVal(GiExpectedResultsPanel,	EXPRESULTS_VALUE,	variable);
					}
					if(Valrangevalue == 2)
					{
						count_pass++;
						if(count_pass == 1)
						{
							GetCtrlVal(GiExpectedResultsPanel,	EXPRESULTS_FORMUEXPEC,	Formule2); //insert
							//strtok(Formule2, "");
							GetCtrlVal(GiExpectedResultsPanel,	EXPRESULTS_VALUE,	variable);
							strcat(Formule2, ":");
							strcat(variable,Formule2);
							SetCtrlVal(GiExpectedResultsPanel,	EXPRESULTS_VALUE,	variable);
						}
						if(count_pass == 2)
						{
							GetCtrlVal(GiExpectedResultsPanel,	EXPRESULTS_FORMUEXPEC,	Formule2); //insert
							GetCtrlVal(GiExpectedResultsPanel,	EXPRESULTS_VALUE,	variable);
							strcat(variable, Formule2);
							SetCtrlVal(GiExpectedResultsPanel,	EXPRESULTS_VALUE,	variable);
							Valrangevalue = 0;
						}

					}
					if(Valonevalue == 3)
					{
						count_pass++;
						if(count_pass==1)
						{
							GetCtrlVal(GiExpectedResultsPanel,	EXPRESULTS_FORMUEXPEC,	Formule2); //insert
							GetCtrlVal(GiExpectedResultsPanel,	EXPRESULTS_VALUE,	variable);
							strcat(variable, Formule2);
							SetCtrlVal(GiExpectedResultsPanel,	EXPRESULTS_VALUE,	variable);
						}
					}


					break;

				case 12: //Tol text box
					SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE2,	Formule2);
					SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE1,	"0"); 
					break;

				case 13: //TolPer text box
					SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE1,	Formule2);
					SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE2,	"0"); 
					break;

				case 14: //Function Code Value text box 		   Formule2
					SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_VALUEFC,	Formule2);
					/*SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTA,ATTR_VISIBLE, 0);
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTB,ATTR_VISIBLE, 0);
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTC,ATTR_VISIBLE, 0);
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTD,ATTR_VISIBLE, 0);
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTE,ATTR_VISIBLE, 0);
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTF,ATTR_VISIBLE, 0);*/
					//MARVYN!

					break;

					// operator doesn't match any case
				default:
					printf("Error! operator is not correct");
			}
			

			//Modif MaximePAGES 30/07/2020
			//Value2 = parameterToValue (Formule2withoutVariable);
			if ( currentTxtBox == 14 )
			{
				Value2 = 0.0;	
			}
			else 
			{
				Value2 = atof(parameterToValue_Str(Formule2));    //Modif MaximePAGES 05/08/2020
			}


			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_EXPVALUE_2, Value2);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,"");
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_EXPVALUE,"");
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_EXPUNIT,"");

			
			if ( (Valsequencevalue == 1) || (Valrangevalue == 2) ) 
			{
				//	
			} 
			else 
				dimmedNumericKeypadForExp(1);

	}

	return 0;
}

//***********************************************************************************************
// Modif carolina 03/2019
//-------------------------------------BUTTONS FORMULE-------------------------------------------
int CVICALLBACK butpar1 (int panel, int control, int event,
						 void *callbackData, int eventData1, int eventData2)
{
	char Param[500]=" ( ";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			strcat(Formula, Param);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);

			break;
	}
	return 0;
}

int CVICALLBACK butpar2 (int panel, int control, int event,
						 void *callbackData, int eventData1, int eventData2)
{
	char Param[500]=" ) ";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			strcat(Formula, Param);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			break;
	}
	return 0;
}

int CVICALLBACK butsum (int panel, int control, int event,
						void *callbackData, int eventData1, int eventData2)
{
	char Param[500]=" + ";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			strcat(Formula, Param);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			break;
	}
	return 0;
}

int CVICALLBACK butsub (int panel, int control, int event,
						void *callbackData, int eventData1, int eventData2)
{
	char Param[500]=" - ";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			strcat(Formula, Param);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			break;
	}
	return 0;
}

int CVICALLBACK butmult (int panel, int control, int event,
						 void *callbackData, int eventData1, int eventData2)
{
	char Param[500]=" * ";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			strcat(Formula, Param);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			break;
	}
	return 0;
}

int CVICALLBACK butdiv (int panel, int control, int event,
						void *callbackData, int eventData1, int eventData2)
{
	char Param[500]=" / ";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			strcat(Formula, Param);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			break;
	}
	return 0;
}




//Modif MaximePAGES 30/07/2020 -new IHM parameters  numeric keypad for Expected Results Panel  ********

int CVICALLBACK butdot (int panel, int control, int event,
						void *callbackData, int eventData1, int eventData2)
{
	char Param[500]=".";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			strcat(Formula, Param);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			break;
	}
	return 0;
}


int CVICALLBACK but0 (int panel, int control, int event,
					  void *callbackData, int eventData1, int eventData2)
{
	char Param[500]="0";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			strcat(Formula, Param);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			break;
	}
	return 0;
}

int CVICALLBACK but1 (int panel, int control, int event,
					  void *callbackData, int eventData1, int eventData2)
{
	char Param[500]="1";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			strcat(Formula, Param);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			break;
	}
	return 0;
}

int CVICALLBACK but2 (int panel, int control, int event,
					  void *callbackData, int eventData1, int eventData2)
{
	char Param[500]="2";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			strcat(Formula, Param);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			break;
	}
	return 0;
}

int CVICALLBACK but3 (int panel, int control, int event,
					  void *callbackData, int eventData1, int eventData2)
{
	char Param[500]="3";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			strcat(Formula, Param);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			break;
	}
	return 0;
}

int CVICALLBACK but4 (int panel, int control, int event,
					  void *callbackData, int eventData1, int eventData2)
{
	char Param[500]="4";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			strcat(Formula, Param);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			break;
	}
	return 0;
}

int CVICALLBACK but5 (int panel, int control, int event,
					  void *callbackData, int eventData1, int eventData2)
{
	char Param[500]="5";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			strcat(Formula, Param);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			break;
	}
	return 0;
}

int CVICALLBACK but6 (int panel, int control, int event,
					  void *callbackData, int eventData1, int eventData2)
{
	char Param[500]="6";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			strcat(Formula, Param);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			break;
	}
	return 0;
}

int CVICALLBACK but7 (int panel, int control, int event,
					  void *callbackData, int eventData1, int eventData2)
{
	char Param[500]="7";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			strcat(Formula, Param);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			break;
	}
	return 0;
}

int CVICALLBACK but8 (int panel, int control, int event,
					  void *callbackData, int eventData1, int eventData2)
{
	char Param[500]="8";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			strcat(Formula, Param);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			break;
	}
	return 0;
}

int CVICALLBACK but9 (int panel, int control, int event,
					  void *callbackData, int eventData1, int eventData2)
{
	char Param[500]="9";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			strcat(Formula, Param);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			break;
	}
	return 0;
}

int CVICALLBACK butA (int panel, int control, int event,
					  void *callbackData, int eventData1, int eventData2)
{
	char Param[500]="A";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			strcat(Formula, Param);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			break;
	}
	return 0;
}

int CVICALLBACK butB (int panel, int control, int event,
					  void *callbackData, int eventData1, int eventData2)
{
	char Param[500]="B";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			strcat(Formula, Param);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			break;
	}
	return 0;
}

int CVICALLBACK butC (int panel, int control, int event,
					  void *callbackData, int eventData1, int eventData2)
{
	char Param[500]="C";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			strcat(Formula, Param);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			break;
	}
	return 0;
}

int CVICALLBACK butD (int panel, int control, int event,
					  void *callbackData, int eventData1, int eventData2)
{
	char Param[500]="D";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			strcat(Formula, Param);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			break;
	}
	return 0;
}

int CVICALLBACK butE (int panel, int control, int event,
					  void *callbackData, int eventData1, int eventData2)
{
	char Param[500]="E";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			strcat(Formula, Param);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			break;
	}
	return 0;
}

int CVICALLBACK butF (int panel, int control, int event,
					  void *callbackData, int eventData1, int eventData2)
{
	char Param[500]="F";
	char Formula[500]="";

	switch (event)
	{
		case EVENT_COMMIT:
			GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			strcat(Formula, Param);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,Formula);
			break;
	}
	return 0;
}



int CVICALLBACK wuid_fonction_exp (int panel, int control, int event,
							   void *callbackData, int eventData1, int eventData2)
{
	int itemValue=10;
	char range[30];
	char valueParameter[100];
	//char unitParameter[100];
	char valueDefault[100]=" ";

	switch (event)
	{
		case EVENT_VAL_CHANGED:

			//SetCtrlVal(panel,PANEL_ADD_WUID_VALUE,"");
			GetCtrlVal(panel,EXPRESULTS_WUIDEXP, &itemValue);

			if (itemValue==0)
			{
				SetCtrlVal(panel,EXPRESULTS_WUID_VALUE,valueDefault);
			}
			else
			{
				sprintf(range,"P%d",itemValue+1);
				ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, valueParameter);
				SetCtrlVal(panel,EXPRESULTS_WUID_VALUE,valueParameter);
			}
			break;
	}
	return 0;
}





//*****************************************************************************************************


int CVICALLBACK selectparam (int panel, int control, int event,
							 void *callbackData, int eventData1, int eventData2)
{
	
	char valueParameter[100];
	char unitParameter[100];


	char Formula[500]="";
	

	int itemValue=0;
	/*
	switch (event)
	{
		case EVENT_VAL_CHANGED:

			//SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_SELECTPARAM,iselectparam);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,"");
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_EXPUNIT,"");
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_EXPVALUE,"");

			//ExcelRpt_ApplicationNew(VFALSE,&applicationHandle);
			//ExcelRpt_WorkbookOpen (applicationHandleProject, GsCheckCfgFile, &workbookHandledata);
			//ExcelRpt_GetWorksheetFromIndex(workbookHandledata, 2, &worksheetHandledata2);

			GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_SELECTPARAM, &itemValue);

			sprintf(range,"B%d",itemValue+1); //colonne B valeurs
			ExcelRpt_GetCellValue (worksheetHandledata2, range, CAVT_CSTRING, valueParameter);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_EXPVALUE,valueParameter);

			sprintf(range,"C%d",itemValue+1); //colonne C unités
			ExcelRpt_GetCellValue (worksheetHandledata2, range, CAVT_CSTRING, unitParameter);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_EXPUNIT,unitParameter);

			if( itemValue == 0)
			{
			}
			else
			{
				GetCtrlVal(GiExpectedResultsPanel, 			EXPRESULTS_SELECTPARAM, 	&ListIndex);
				GetLabelFromIndex(GiExpectedResultsPanel,	EXPRESULTS_SELECTPARAM, 	ListIndex, Param);
				GetCtrlVal(GiExpectedResultsPanel,			EXPRESULTS_EXPVALUE, 		sValue);
				iValue = atoi(sValue);

				GetCtrlVal(GiExpectedResultsPanel,			EXPRESULTS_FORMUEXPEC, 		Formula);
				strcat(Formula, " ");
				strcat(Formula, Param);
				SetCtrlVal(GiExpectedResultsPanel,			EXPRESULTS_FORMUEXPEC, 		Formula);

			}
			break;


	}
	*/
	
	switch (event)
	{
		case EVENT_VAL_CHANGED:
			
			GetCtrlVal(panel,EXPRESULTS_SELECTPARAM, &itemValue);

			if( itemValue == 0)
			{
				 SetCtrlVal(panel,EXPRESULTS_EXPVALUE,""); 
				 SetCtrlVal(panel,EXPRESULTS_EXPUNIT,"") ;
				
			}
			else
			{

				strcpy(valueParameter,myParameterTab[itemValue-1].value);
				SetCtrlVal(panel,EXPRESULTS_EXPVALUE,valueParameter);

				strcpy(unitParameter,myParameterTab[itemValue-1].unit) ;
				SetCtrlVal(panel,EXPRESULTS_EXPUNIT,unitParameter);

				GetCtrlVal(panel,EXPRESULTS_FORMUEXPEC, Formula);
				strcat(Formula, " ");
				strcat(Formula, myParameterTab[itemValue-1].name);
				SetCtrlVal(panel,EXPRESULTS_FORMUEXPEC, Formula);
			}
			
			break;
	}
	

	return 0;
}

//-------------------------------------FIN----------------------------------------------

//***********************************************************************************************
// Modif - Carolina 04/2019
// translate of params conti to params costumer
//***********************************************************************************************
int CVICALLBACK fieldtocheck (int panel, int control, int event,
							  void *callbackData, int eventData1, int eventData2)
{
	int itemValue=0, itemCheck;
	char range[30];
	char realParameter[255] = "";
	char parameter[255] = "";
	char itemSelected[255] = "";
	int parameterCompare = 0;
	int boucle = 0, end = 1;
	//char endCondition[2] = "";
	int ListIndex = 0;


	switch (event)
	{
		case EVENT_VAL_CHANGED:
			//GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK, &itemValue);

			GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_EXPLIST, &itemCheck);
			GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA, &itemValue);  //to take the good column
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_ATTRIBUTENAME,"");

			// Ouverture de la database et récuprération des paramètres
			//ExcelRpt_ApplicationNew(VFALSE,&applicationHandledata);
			//ExcelRpt_WorkbookOpen (applicationHandleProject, GsCheckCfgFile, &workbookHandledata);
			//ExcelRpt_GetWorksheetFromIndex(workbookHandledata, 1, &worksheetHandledata1);  //Worksheet Configuration

			switch (itemValue)
			{
				case 0:
					//SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,	ATTR_DIMMED,1);
					break;

				case 1:  //stand_Frm
					GetCtrlVal(panel, EXPRESULTS_FIELDCHECK, &ListIndex);
					GetLabelFromIndex(panel,EXPRESULTS_FIELDCHECK, ListIndex, itemSelected);

					//load column A and compare
					for(boucle=2; end==1; boucle++) //A3 until ""
					{
						sprintf(range,"A%d",boucle+1);  //start with A3
						ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

						//stop condition
						parameterCompare = strcmp(parameter,itemSelected);	// compare les deux chaine de caractère

						if(parameterCompare == 0) //when parameter is ""
						{
							//get the word in the next column
							sprintf(range,"B%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, realParameter);
							SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_ATTRIBUTENAME, realParameter);
							SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_TAKEFIELD, itemSelected);  //this field is invisible
							end = 0;
						}
						else
						{
							char notFound[2] = "";
							SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_TAKEFIELD, notFound);
						}
					}

					end=1;

					break;

				case 2:   //Diag_Frm
					//load column C
					GetCtrlVal(panel, EXPRESULTS_FIELDCHECK, &ListIndex);
					GetLabelFromIndex(panel,EXPRESULTS_FIELDCHECK, ListIndex, itemSelected);

					for(boucle=2; end==1; boucle++)
					{
						sprintf(range,"C%d",boucle+1);
						ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

						//stop condition
						parameterCompare = strcmp(parameter,itemSelected);	// compare les deux chaine de caractère

						if(parameterCompare == 0) //when parameter is ""
						{
							//get the word in the next column
							sprintf(range,"D%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, realParameter);
							SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_ATTRIBUTENAME, realParameter);
							SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_TAKEFIELD, parameter);
							end = 0;
						}
						else
						{
							char notFound[2] = "";
							SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_TAKEFIELD, notFound);
						}
					}
					end=1;


					break;

				case 3: //WUP_Frm
					GetCtrlVal(panel, EXPRESULTS_FIELDCHECK, &ListIndex);
					GetLabelFromIndex(panel,EXPRESULTS_FIELDCHECK, ListIndex, itemSelected);

					for(boucle=2; end==1; boucle++)
					{
						sprintf(range,"E%d",boucle+1);
						ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

						//stop condition
						parameterCompare = strcmp(parameter,itemSelected);	// compare les deux chaine de caractère

						if(parameterCompare == 0) //when parameter is ""
						{
							//get the word in the next column
							sprintf(range,"F%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, realParameter);
							SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_ATTRIBUTENAME, realParameter);
							SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_TAKEFIELD, parameter);
							end = 0;
						}
						else
						{
							char notFound[2] = "";
							SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_TAKEFIELD, notFound);
						}
					}

					end=1;

					break;

				case 4:  //LSE
					//load column G
					GetCtrlVal(panel, EXPRESULTS_FIELDCHECK, &ListIndex);
					GetLabelFromIndex(panel,EXPRESULTS_FIELDCHECK, ListIndex, itemSelected);

					for(boucle=2; end==1; boucle++)
					{
						sprintf(range,"G%d",boucle+1);
						ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

						//stop condition
						parameterCompare = strcmp(parameter,itemSelected);	// compare les deux chaine de caractère

						if(parameterCompare == 0) //when parameter is ""
						{
							//get the word in the next column
							sprintf(range,"H%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, realParameter);
							SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_ATTRIBUTENAME, realParameter);
							SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_TAKEFIELD, parameter);
							end = 0;
						}
						else
						{
							char notFound[2] = "";
							SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_TAKEFIELD, notFound);
						}
					}

					end=1;

					break;

				case 5:  //Druck
					//load column I
					GetCtrlVal(panel, EXPRESULTS_FIELDCHECK, &ListIndex);
					GetLabelFromIndex(panel,EXPRESULTS_FIELDCHECK, ListIndex, itemSelected);

					for(boucle=2; end==1; boucle++) //I3 until ""
					{
						sprintf(range,"I%d",boucle+1);
						ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

						//stop condition
						parameterCompare = strcmp(parameter,itemSelected);	// compare les deux chaine de caractère

						if(parameterCompare == 0) //when parameter is ""
						{
							//get the word in the next column
							sprintf(range,"J%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, realParameter);
							SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_ATTRIBUTENAME, realParameter);
							SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_TAKEFIELD, parameter);
							end = 0;
						}
						else
						{
							char notFound[2] = "";
							SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_TAKEFIELD, notFound);
						}
					}

					end=1;

					break;


				case 6:	   //SW_Ident
					//load column K
					GetCtrlVal(panel, EXPRESULTS_FIELDCHECK, &ListIndex);
					GetLabelFromIndex(panel,EXPRESULTS_FIELDCHECK, ListIndex, itemSelected);

					for(boucle=2; end==1; boucle++)
					{
						sprintf(range,"K%d",boucle+1);
						ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

						//stop condition
						parameterCompare = strcmp(parameter,itemSelected);	// compare les deux chaine de caractère

						if(parameterCompare == 0) //when parameter is ""
						{
							//get the word in the next column
							sprintf(range,"L%d",boucle+1);
							ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, realParameter);
							SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_ATTRIBUTENAME, realParameter);
							SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_TAKEFIELD, parameter);
							end = 0;
						}
						else
						{
							char notFound[2] = "";
							SetCtrlVal (GiExpectedResultsPanel, EXPRESULTS_TAKEFIELD, notFound);
						}
					}

					end=1;

					break;
			}


			break;

	}
	ListIndex=0;
	boucle = 0;
	return 0;
}

//***********************************************************************************************
// Modif - Carolina 04/2019
// Get params from the right interface
//***********************************************************************************************
int CVICALLBACK parametersdata (int panel, int control, int event,
								void *callbackData, int eventData1, int eventData2)
{
	char range[30];
	int itemValue=0, ifieldcheck=0;
	char endCondition[2] = "";
	int boucle = 0, end = 1;
	char parameter[100] = "";
	int parameterCompare = 0;
	char *column="";

	switch (event)
	{
		case EVENT_VAL_CHANGED:


			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,ifieldcheck);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_ATTRIBUTENAME,"");
			InsertListItem (GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,0,"",0);

			//ExcelRpt_ApplicationNew(VFALSE,&applicationHandledata);
			//ExcelRpt_WorkbookOpen (applicationHandleProject, GsCheckCfgFile, &workbookHandledata); // GsCheckCfgFile ou "C:\\Users\\uid42961\\Desktop\\WU TestBench\\Automation SYP.xlsx"
			//ExcelRpt_GetWorksheetFromIndex(workbookHandledata, 1, &worksheetHandledata1);  //Worksheet Configuration

			GetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_PARAMETERSDATA, &itemValue);
		if (fieldNeeded == 1)
		{
			switch (itemValue)
			{
				case 0:
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,	ATTR_DIMMED,1);
					boucle = 0;
					//InsertListItem (GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,0,"",);
					SetCtrlIndex (GiExpectedResultsPanel, EXPRESULTS_FIELDCHECK, 0);
					break;

				case 1:  
					
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,		ATTR_DIMMED,0);
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ATTRIBUTENAME,	ATTR_DIMMED,0);
					//load column A

					for(boucle=2; end==1; boucle++) //A3 until ""
					{
						sprintf(range,"%s%d",Interfaces[0].columnParam,boucle+1);  //start with A3
						//printf("range = %s\n",range);
						ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

						//stop condition
						parameterCompare = strcmp(parameter,endCondition);	// compare les deux chaine de caractère

						if(parameterCompare == 0) //when parameter is ""
						{
							end=0;
						}
						else
						{
							InsertListItem (GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,boucle-1,parameter,boucle-1);
						}
					}

					end=1;

					break;

				case 2:  
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,		ATTR_DIMMED,0);
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ATTRIBUTENAME,	ATTR_DIMMED,0);

					//load column C
					for(boucle=2; end==1; boucle++) //C3 until ""
					{
						sprintf(range,"%s%d",Interfaces[1].columnParam,boucle+1);
						//printf("range = %s\n",range);
						ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

						//stop condition
						parameterCompare = strcmp(parameter,endCondition);	// compare les deux chaine de caractère

						if(parameterCompare == 0) //when parameter is ""
						{
							end=0;
						}
						else
						{
							InsertListItem (GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,boucle-1,parameter,boucle-1);
						}
					}

					end=1;


					break;

				case 3:   
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,		ATTR_DIMMED,0);
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ATTRIBUTENAME,	ATTR_DIMMED,0);

					//load column E
					for(boucle=2; end==1; boucle++) //E3 until ""
					{
						sprintf(range,"%s%d",Interfaces[2].columnParam,boucle+1);
						//printf("range = %s\n",range);
						ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

						//stop condition
						parameterCompare = strcmp(parameter,endCondition);	// compare les deux chaine de caractère

						if(parameterCompare == 0) //when parameter is ""
						{
							end=0;
						}
						else
						{
							InsertListItem (GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,boucle-1,parameter,boucle-1);
						}
					}

					end=1;

					break;

				case 4:	   
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,		ATTR_DIMMED,0);
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ATTRIBUTENAME,	ATTR_DIMMED,0);

					//load column G
					for(boucle=2; end==1; boucle++) //G2 until ""
					{
						sprintf(range,"%s%d",Interfaces[3].columnParam,boucle+1);
						//printf("range = %s\n",range);
						ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

						//stop condition
						parameterCompare = strcmp(parameter,endCondition);	// compare les deux chaine de caractère

						if(parameterCompare == 0) //when parameter is ""
						{
							end=0;
						}
						else
						{
							InsertListItem (GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,boucle-1,parameter,boucle-1);
						}
					}

					end=1;

					break;

				case 5:	   
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,		ATTR_DIMMED,0);
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ATTRIBUTENAME,	ATTR_DIMMED,0);

					//load column I
					for(boucle=2; end==1; boucle++) //I2 until ""
					{
						sprintf(range,"%s%d",Interfaces[4].columnParam,boucle+1);
						//printf("range = %s\n",range);
						ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

						//stop condition
						parameterCompare = strcmp(parameter,endCondition);	// compare les deux chaine de caractère

						if(parameterCompare == 0) //when parameter is ""
						{
							end=0;
						}
						else
						{
							InsertListItem (GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,boucle-1,parameter,boucle-1);
						}
					}

					end=1;

					break;


				case 6:		
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,		ATTR_DIMMED,0);
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ATTRIBUTENAME,	ATTR_DIMMED,0);

					//load column K
					for(boucle=2; end==1; boucle++) //K2 until ""
					{
						sprintf(range,"%s%d",Interfaces[5].columnParam,boucle+1);
						//printf("range = %s\n",range);
						ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

						//stop condition
						parameterCompare = strcmp(parameter,endCondition);	// compare les deux chaine de caractère

						if(parameterCompare == 0) //when parameter is ""
						{
							end=0;
						}
						else
						{
							InsertListItem (GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,boucle-1,parameter,boucle-1);
						}
					}

					end=1;

					break;
					
					
				case 7:	   
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,		ATTR_DIMMED,0);
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ATTRIBUTENAME,	ATTR_DIMMED,0);

					//load column I
					for(boucle=2; end==1; boucle++) //I2 until ""
					{
						sprintf(range,"%s%d",Interfaces[6].columnParam,boucle+1);
						//printf("range = %s\n",range);
						ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

						//stop condition
						parameterCompare = strcmp(parameter,endCondition);	// compare les deux chaine de caractère

						if(parameterCompare == 0) //when parameter is ""
						{
							end=0;
						}
						else
						{
							InsertListItem (GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,boucle-1,parameter,boucle-1);
						}
					}

					end=1;

					break;
					
					
				case 8:	   
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,		ATTR_DIMMED,0);
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ATTRIBUTENAME,	ATTR_DIMMED,0);

					//load column I
					for(boucle=2; end==1; boucle++) //I2 until ""
					{
						sprintf(range,"%s%d",Interfaces[7].columnParam,boucle+1);
						//printf("range = %s\n",range);
						ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

						//stop condition
						parameterCompare = strcmp(parameter,endCondition);	// compare les deux chaine de caractère

						if(parameterCompare == 0) //when parameter is ""
						{
							end=0;
						}
						else
						{
							InsertListItem (GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,boucle-1,parameter,boucle-1);
						}
					}

					end=1;

					break;
					
					
				case 9:	   
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,		ATTR_DIMMED,0);
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ATTRIBUTENAME,	ATTR_DIMMED,0);

					//load column I
					for(boucle=2; end==1; boucle++) //I2 until ""
					{
						sprintf(range,"%s%d",Interfaces[8].columnParam,boucle+1);
						//printf("range = %s\n",range);
						ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

						//stop condition
						parameterCompare = strcmp(parameter,endCondition);	// compare les deux chaine de caractère

						if(parameterCompare == 0) //when parameter is ""
						{
							end=0;
						}
						else
						{
							InsertListItem (GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,boucle-1,parameter,boucle-1);
						}
					}

					end=1;

					break;
					
					
				case 10:	   
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,		ATTR_DIMMED,0);
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ATTRIBUTENAME,	ATTR_DIMMED,0);

					//load column I
					for(boucle=2; end==1; boucle++) //I2 until ""
					{
						sprintf(range,"%s%d",Interfaces[9].columnParam,boucle+1);
						//printf("range = %s\n",range);
						ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

						//stop condition
						parameterCompare = strcmp(parameter,endCondition);	// compare les deux chaine de caractère

						if(parameterCompare == 0) //when parameter is ""
						{
							end=0;
						}
						else
						{
							InsertListItem (GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,boucle-1,parameter,boucle-1);
						}
					}

					end=1;

					break;
					
					
				case 11:	   
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,		ATTR_DIMMED,0);
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ATTRIBUTENAME,	ATTR_DIMMED,0);

					//load column I
					for(boucle=2; end==1; boucle++) //I2 until ""
					{
						sprintf(range,"%s%d",Interfaces[10].columnParam,boucle+1);
						//printf("range = %s\n",range);
						ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

						//stop condition
						parameterCompare = strcmp(parameter,endCondition);	// compare les deux chaine de caractère

						if(parameterCompare == 0) //when parameter is ""
						{
							end=0;
						}
						else
						{
							InsertListItem (GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,boucle-1,parameter,boucle-1);
						}
					}

					end=1;

					break;
					
					
				case 12:	   
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,		ATTR_DIMMED,0);
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ATTRIBUTENAME,	ATTR_DIMMED,0);

					//load column I
					for(boucle=2; end==1; boucle++) //I2 until ""
					{
						sprintf(range,"%s%d",Interfaces[11].columnParam,boucle+1);
						//printf("range = %s\n",range);
						ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

						//stop condition
						parameterCompare = strcmp(parameter,endCondition);	// compare les deux chaine de caractère

						if(parameterCompare == 0) //when parameter is ""
						{
							end=0;
						}
						else
						{
							InsertListItem (GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,boucle-1,parameter,boucle-1);
						}
					}

					end=1;

					break;
					
					
				case 13:	   
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,		ATTR_DIMMED,0);
					SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_ATTRIBUTENAME,	ATTR_DIMMED,0);

					//load column I
					for(boucle=2; end==1; boucle++) //I2 until ""
					{
						sprintf(range,"%s%d",Interfaces[12].columnParam,boucle+1);
						//printf("range = %s\n",range);
						ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, parameter);

						//stop condition
						parameterCompare = strcmp(parameter,endCondition);	// compare les deux chaine de caractère

						if(parameterCompare == 0) //when parameter is ""
						{
							end=0;
						}
						else
						{
							InsertListItem (GiExpectedResultsPanel,EXPRESULTS_FIELDCHECK,boucle-1,parameter,boucle-1);
						}
					}

					end=1;

					break;
			}

			break;
		}
	}

	return 0;
}

//***********************************************************************************************
// Modif - Carolina 05/2019
// reset parameters
//***********************************************************************************************
int CVICALLBACK deletebutton (int panel, int control, int event,
							  void *callbackData, int eventData1, int eventData2)
{
	char Formula[500] = "";
	float expvalue2 = 0.0;
	char expvalue[100]="";
	char expunit[100]="";

	switch (event)
	{
		case EVENT_COMMIT:

			DeleteListItem (GiExpectedResultsPanel, EXPRESULTS_FIELDCHECK, 0, -1);

			SetCtrlVal(GiExpectedResultsPanel, EXPRESULTS_FORMUEXPEC, 	Formula);
			SetCtrlVal(GiExpectedResultsPanel, EXPRESULTS_EXPVALUE_2, 	expvalue2);
			SetCtrlVal(GiExpectedResultsPanel, EXPRESULTS_EXPVALUE, 	expvalue);
			SetCtrlVal(GiExpectedResultsPanel, EXPRESULTS_EXPUNIT, 		expunit);

			break;
	}
	return 0;
}

//***********************************************************************************************
// Modif - Carolina 05/2019
// reset parameters
//***********************************************************************************************


// Modif Maxime PAGES - 27/07/2020 new IHM parameters numeric keypad
int CVICALLBACK boutondel (int panel, int control, int event,
						   void *callbackData, int eventData1, int eventData2)
{
	char Formula[500] = "";
	float duration = 0;
	float time = 0;
	char addvalue[100]="";
	char addunit[100]="";

	switch (event)
	{
		case EVENT_COMMIT:

			SetCtrlAttribute (GiPopupAdd2, PANEL_ADD_PARAMETERS, ATTR_DFLT_VALUE, 0);
			DefaultCtrl (GiPopupAdd2, PANEL_ADD_PARAMETERS);

			SetCtrlVal(GiPopupAdd2, PANEL_ADD_FORMULA, 		Formula);
			SetCtrlVal(GiPopupAdd2, PANEL_ADD_DURATION, 	duration);
			SetCtrlVal(GiPopupAdd2, PANEL_ADD_TIME, 		time);
			SetCtrlVal(GiPopupAdd2, PANEL_ADD_VALUE, 		addvalue);
			SetCtrlVal(GiPopupAdd2, PANEL_ADD_UNIT, 		addunit);

			break;
	}
	return 0;
}


int CVICALLBACK exp_value (int panel, int control, int event,
						   void *callbackData, int eventData1, int eventData2)
{

	char variable[255] = "";
	char auxvariable[255] = "";

	if(add_val == 1) //no click on insert formule
	{


		GetCtrlVal(GiExpectedResultsPanel,	EXPRESULTS_VALUE, variable);
		if(strcmp(variable, "") != 0)   //not empty
		{
			strcpy(auxvariable, variable);
			char *token = strtok(auxvariable , ";");
			if(token)
			{
				event_sequence = 1;
			}
		}

	}


	return 0;
}

int CVICALLBACK delvalue (int panel, int control, int event,
						  void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{
		case EVENT_COMMIT:
			SetCtrlVal(GiExpectedResultsPanel, EXPRESULTS_VALUE, 	"");
			SetCtrlVal(GiPanelValue, PANELVALUE_SEQUENCEVALUE, 		0);
			SetCtrlVal(GiPanelValue, PANELVALUE_RANGEVALUE, 		0);
			SetCtrlVal(GiPanelValue, PANELVALUE_ONEVALUE, 			0);
			Valsequencevalue = 0;
			Valonevalue = 0;
			Valrangevalue = 0;
			count_pass=0;
			add_val = 0;
			
			dimmedNumericKeypadForExp(1);
			
			break;
	}
	return 0;
}

int CVICALLBACK deltol(int panel, int control, int event,
						  void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{
		case EVENT_COMMIT:
			SetCtrlVal(GiExpectedResultsPanel, EXPRESULTS_TOLERENCE2, 	"0");
			
			dimmedNumericKeypadForExp(1);
			
			break;
	}
	return 0;
}


int CVICALLBACK deltolper (int panel, int control, int event,
						  void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{
		case EVENT_COMMIT:
			SetCtrlVal(GiExpectedResultsPanel, EXPRESULTS_TOLERENCE1, 	"0");
			
			dimmedNumericKeypadForExp(1);
			
			break;
	}
	return 0;
}




//***********************************************************************************************
// Modif - Carolina 07/2019
// type of values
//***********************************************************************************************
int CVICALLBACK okbutton (int panel, int control, int event,
						  void *callbackData, int eventData1, int eventData2)
{
	int valueBox=0;



	switch (event)
	{
		case EVENT_COMMIT:

			HidePanel(GiPanelValue);

			GetCtrlVal(GiPanelValue,PANELVALUE_OKBUTTON,&valueBox);
			if(Valonevalue != 0 || Valsequencevalue !=0 ||  Valrangevalue !=0)
			{
				dimmedNumericKeypadForExp(0); //Modif MaximePAGES 30/07/2020 - New IHM Numeric Keypad
			}

			break;
	}
	return 0;
}



int CVICALLBACK sequencevalue (int panel, int control, int event,
							   void *callbackData, int eventData1, int eventData2)
{

	switch (event)
	{
		case EVENT_COMMIT:
			
			SetCtrlVal(GiPanelValue, PANELVALUE_ONEVALUE, 	0);  
			SetCtrlVal(GiPanelValue, PANELVALUE_SEQUENCEVALUE, 	1);
			SetCtrlVal(GiPanelValue, PANELVALUE_RANGEVALUE, 		0);
			
			
			Valsequencevalue = 1;
			Valonevalue = 0;
			Valrangevalue = 0;	
			
			SetCtrlAttribute(GiPanelValue,PANELVALUE_OKBUTTON,		ATTR_DIMMED, 0);  
			
			break;
	}
	return 0;
}

int CVICALLBACK rangevalue (int panel, int control, int event,
							void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{
		case EVENT_COMMIT:
			
			SetCtrlVal(GiPanelValue, PANELVALUE_ONEVALUE, 	0);  
			SetCtrlVal(GiPanelValue, PANELVALUE_SEQUENCEVALUE, 	0);
			SetCtrlVal(GiPanelValue, PANELVALUE_RANGEVALUE, 		1);
			
			Valrangevalue = 2;
			Valsequencevalue = 0;
			Valonevalue = 0;

			SetCtrlAttribute(GiPanelValue,PANELVALUE_OKBUTTON,		ATTR_DIMMED, 0); 

			break;
	}
	return 0;
}

int CVICALLBACK onevalue (int panel, int control, int event,
						  void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{
		case EVENT_COMMIT:
			
			SetCtrlVal(GiPanelValue, PANELVALUE_ONEVALUE, 	1);  
			SetCtrlVal(GiPanelValue, PANELVALUE_SEQUENCEVALUE, 	0);
			SetCtrlVal(GiPanelValue, PANELVALUE_RANGEVALUE, 		0);

			Valonevalue = 3;
			Valsequencevalue = 0;
			Valrangevalue = 0;
			
			SetCtrlAttribute(GiPanelValue,PANELVALUE_OKBUTTON,		ATTR_DIMMED, 0);  

			break;
	}
	return 0;
}

//***********************************************************************************************
// Modif - Carolina 03/2019
// The button "save file" saves the test script and the expected results in a workbook
//***********************************************************************************************

int CVICALLBACK savefilesbutton (int panel, int control, int event,
								 void *callbackData, int eventData1, int eventData2)
{
	Point cell;
	char range[30];
	char sCellValue[200]="";
	char worksheetName2[20]="ExpectedResults";
	char worksheetName1[10]="TestCase";
	char rangeColor[30];
	char *sPre=NULL;
	char *sScript=NULL;
	char *sPost=NULL;
	char defaultDirectoryER[MAX_PATHNAME_LEN] = "D:\\";
	char currentDirectory[MAX_PATHNAME_LEN];
	int boucle1=0, boucle2=0;
	//float sum=0;
	//int itime =0;
	//int iduration=0;
	//float finalsum=0;
	float reset_endtime = 0;
	int errDir = 0;


	GetDir(defaultDirectoryER); //AJOUT MaximePAGES - 15/06/2020 E103

	errDir = GetDir(currentDirectory);
	if(errDir == -3 || errDir == -4)
	{
		FmtOut ("Project is untitled\n");
		strcpy(currentDirectory, defaultDirectoryER);
	}

	switch (event)
	{
		case EVENT_COMMIT:



			if (mode == 1) //Execution Mode

			{


				char *confpath;
				char ProjectDirectory[MAX_PATHNAME_LEN];
				int iRet=0;



				GetProjectDir(ProjectDirectory);
				confpath = strcat(ProjectDirectory,"\\Config");
				SaveConfiguration(iRet, confpath);

			}
			else   //Creation Mode
			{


				//check if there is at least one line in the expected results
				if(((*PointeurNbRowsExpR) == 0) && ((*pointeurval) == 0))
				{
					MessagePopup ("Warning", "No files created\nPlease create a Test script or an Expected results");
				}
				else
				{
					//HidePanel(GiPopupAdd2);
					//HidePanel(GiExpectedResultsPanel);
					float endtimeplus = 0;

					if(((*pointeurval+1) >= 2) || ((*PointeurNbRowsExpR+1) >= 2))
					{
						//create sheets
						//Ouverture Excel pour script de test
						//ExcelRpt_ApplicationNew(VTRUE,&applicationHandlesave);
						ExcelRpt_WorkbookNew (applicationHandleProject, &workbookHandlesave);
						ExcelRpt_GetWorksheetFromIndex(workbookHandlesave, 1, &worksheetHandlesave1);
						ExcelRpt_WorksheetNew (workbookHandlesave, -1, &worksheetHandlesave2);
						ExcelRpt_SetWorksheetAttribute (worksheetHandlesave1, ER_WS_ATTR_NAME, worksheetName1); // Nom de la worksheet : TestCase
						ExcelRpt_SetWorksheetAttribute (worksheetHandlesave2, ER_WS_ATTR_NAME, worksheetName2);	//ExpectedResults

						//------------------------------------------------------------------------------------------------------
						//Save File from Test Script
						//------------------------------------------------------------------------------------------------------
						GetCtrlVal(GiPanel, PANEL_MODE_ENDTIME, &endtimeplus);

						// Fermeture SYP
						//ExcelRpt_WorkbookClose (workbookHandle, 0);
						//ExcelRpt_ApplicationQuit (applicationHandle);

						// Esthétique excel
						ExcelRpt_SetCellRangeAttribute (worksheetHandlesave1, "A1:H1", ER_CR_ATTR_FONT_BOLD, 1);
						ExcelRpt_SetCellRangeAttribute (worksheetHandlesave1, "A1:H1", ER_CR_ATTR_BGCOLOR, 0xC0C0C0L);
						ExcelRpt_SetCellRangeAttribute (worksheetHandlesave1, "A1:H1", ER_CR_ATTR_HORIZ_ALIGN, ExRConst_HAlignCenter);
						ExcelRpt_SetCellRangeAttribute (worksheetHandlesave1, "A2:H100", ER_CR_ATTR_HORIZ_ALIGN, ExRConst_HAlignCenter);

						ExcelRpt_RangeBorder (worksheetHandlesave1, "A1", 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);
						ExcelRpt_RangeBorder (worksheetHandlesave1, "B1", 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);
						ExcelRpt_RangeBorder (worksheetHandlesave1, "C1", 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);
						ExcelRpt_RangeBorder (worksheetHandlesave1, "D1", 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);
						ExcelRpt_RangeBorder (worksheetHandlesave1, "E1", 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);
						ExcelRpt_RangeBorder (worksheetHandlesave1, "F1", 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);
						ExcelRpt_RangeBorder (worksheetHandlesave1, "G1", 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);
						ExcelRpt_RangeBorder (worksheetHandlesave1, "H1", 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);

						// Nom de la première ligne excel
						ExcelRpt_SetCellValue (worksheetHandlesave1,"A1" , CAVT_CSTRING,"Time");
						ExcelRpt_SetCellValue (worksheetHandlesave1,"B1" , CAVT_CSTRING,"Duration");
						ExcelRpt_SetCellValue (worksheetHandlesave1,"C1" , CAVT_CSTRING,"Function");
						ExcelRpt_SetCellValue (worksheetHandlesave1,"D1" , CAVT_CSTRING,"Value");
						ExcelRpt_SetCellValue (worksheetHandlesave1,"E1" , CAVT_CSTRING,"Nb Frame");
						ExcelRpt_SetCellValue (worksheetHandlesave1,"F1" , CAVT_CSTRING,"WU ID");
						ExcelRpt_SetCellValue (worksheetHandlesave1,"G1" , CAVT_CSTRING,"Interframe");
						ExcelRpt_SetCellValue (worksheetHandlesave1,"H1" , CAVT_CSTRING,"Comment");

						//Nb lignes
						ExcelRpt_SetCellValue (worksheetHandlesave1,"J1" , CAVT_CSTRING,"(nb of rows)");
						ExcelRpt_SetCellValue (worksheetHandlesave1,"I1" , CAVT_INT,*pointeurval+1);

						// Enregistrement sur le tableau excel par ligne
						for( boucle1 =1; boucle1 <= *pointeurval; boucle1++)
						{
							//printf("i am in script loop \n");
							//taking time and duration
							cell.y = boucle1;
							cell.x =1;
							sprintf(range,"A%d",boucle1+1);
							GetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
							ExcelRpt_SetCellValue (worksheetHandlesave1, range, CAVT_CSTRING,sCellValue);
							ExcelRpt_RangeBorder (worksheetHandlesave1, range, 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);

							cell.x =2;
							sprintf(range,"B%d",boucle1+1);
							GetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
							ExcelRpt_SetCellValue (worksheetHandlesave1,range , CAVT_CSTRING,sCellValue);
							ExcelRpt_RangeBorder (worksheetHandlesave1, range, 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);


							// Modif MaximePAGES 29/07/2020 - new END time definition, -> updateENDTime()
							/*
							itime = findTime(boucle1);  //itime = value1
							iduration = findDuration(boucle1); //iduration = value2

							//calculation of the largest sum
							sum = (float)itime + (float)iduration;
							if(finalsum <= sum)
							{
								finalsum = sum;
								Value1 = 0;
								Value2 = 0;
								itime = 0;
								iduration = 0;
							}
							*/

							cell.x =3;
							sprintf(range,"C%d",boucle1+1);
							GetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
							ExcelRpt_SetCellValue (worksheetHandlesave1,range , CAVT_CSTRING,sCellValue);
							ExcelRpt_RangeBorder (worksheetHandlesave1, range, 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);

							cell.x =4;
							sprintf(range,"D%d",boucle1+1);
							GetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS, cell, sCellValue);
							ExcelRpt_SetCellValue (worksheetHandlesave1,range , CAVT_CSTRING, sCellValue);
							ExcelRpt_RangeBorder (worksheetHandlesave1, range, 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);

							cell.x =5;
							sprintf(range,"E%d",boucle1+1);
							GetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS, cell, sCellValue);
							ExcelRpt_SetCellValue (worksheetHandlesave1,range , CAVT_CSTRING, sCellValue);
							ExcelRpt_RangeBorder (worksheetHandlesave1, range, 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);

							cell.x =6;
							sprintf(range,"F%d",boucle1+1);
							GetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
							ExcelRpt_SetCellValue (worksheetHandlesave1,range , CAVT_CSTRING,sCellValue);
							ExcelRpt_RangeBorder (worksheetHandlesave1, range, 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);

							cell.x =7;
							sprintf(range,"G%d",boucle1+1);
							GetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
							ExcelRpt_SetCellValue (worksheetHandlesave1,range , CAVT_CSTRING,sCellValue);
							ExcelRpt_RangeBorder (worksheetHandlesave1, range, 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);

							cell.x =8;
							sprintf(range,"H%d",boucle1+1);
							GetTableCellVal(panel,PANEL_MODE_SCRIPTS_DETAILS,cell, sCellValue);
							ExcelRpt_SetCellValue (worksheetHandlesave1,range , CAVT_CSTRING,sCellValue);
							ExcelRpt_RangeBorder (worksheetHandlesave1, range, 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);

							// Couleur  en fonction des comments
							sPre=strchr(sCellValue,'e');
							sScript=strchr(sCellValue,'i');
							sPost=strchr(sCellValue,'o');
							sprintf(rangeColor,"A%d:H%d",boucle1+1,boucle1+1);

							if(sPre != NULL)   // vert
							{
								ExcelRpt_SetCellRangeAttribute (worksheetHandlesave1, rangeColor, ER_CR_ATTR_BGCOLOR, 0xA9F79DL);
							}

							if(sScript != NULL) // gris
							{
								ExcelRpt_SetCellRangeAttribute (worksheetHandlesave1, rangeColor, ER_CR_ATTR_BGCOLOR, 0xFFFFFFL);
							}

							if(sPost != NULL)   // bleu
							{
								ExcelRpt_SetCellRangeAttribute (worksheetHandlesave1, rangeColor, ER_CR_ATTR_BGCOLOR, 0x77BBFFL);
							}

							sPre=NULL;
							sScript=NULL;
							sPost=NULL;
						} //for

						sprintf(range,"C%d",boucle1+1);  // Ajoute "END" en fin de table
						ExcelRpt_SetCellValue (worksheetHandlesave1,range , CAVT_CSTRING,"END");
						ExcelRpt_RangeBorder (worksheetHandlesave1, range, 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);

						sprintf(range,"A%d",boucle1+1);  // ajoute le END TIME en fin de table
						//GetCtrlVal(panel,PANEL_MODE_ENDTIME,&iCellValue);
						//ExcelRpt_SetCellValue (worksheetHandle3,range , CAVT_INT,iCellValue);

						//Modif MaximePAGES 29/07/2020 *************************
						//finalsum = finalsum + endtimeplus;
						ExcelRpt_SetCellValue (worksheetHandlesave1,range , CAVT_INT, 0);  //a voir si dans l'execution la dll acepte un float
						ExcelRpt_RangeBorder (worksheetHandlesave1, range, 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);

						//we save the (int)endtimeplus in a separate cell
						ExcelRpt_SetCellValue (worksheetHandlesave1,"J2" , CAVT_CSTRING,"(time to add)");
						ExcelRpt_SetCellValue (worksheetHandlesave1,"I2" , CAVT_INT,(int)endtimeplus);
						//******************************************************


						//ExcelRpt_AddHyperlink (worksheetHandlesave1, "A1", "C:\\Users\\uic80920\\Desktop\\myoutput.txt", "Time_");

						//------------------------------------------------------------------------------------------------------
						//Save File from Expected Results
						//------------------------------------------------------------------------------------------------------

						// Fermeture SYP
						//ExcelRpt_WorkbookClose (workbookHandle4, 0);
						//ExcelRpt_ApplicationQuit (applicationHandle4);


						// Esthétique excel
						ExcelRpt_SetCellRangeAttribute (worksheetHandlesave2, "A1:H1", ER_CR_ATTR_FONT_BOLD, 1);
						ExcelRpt_SetCellRangeAttribute (worksheetHandlesave2, "A1:H1", ER_CR_ATTR_BGCOLOR, 0xC0C0C0L);
						ExcelRpt_SetCellRangeAttribute (worksheetHandlesave2, "A1:H1", ER_CR_ATTR_HORIZ_ALIGN, ExRConst_HAlignCenter);
						ExcelRpt_SetCellRangeAttribute (worksheetHandlesave2, "A2:H100", ER_CR_ATTR_HORIZ_ALIGN, ExRConst_HAlignCenter);

						ExcelRpt_RangeBorder (worksheetHandlesave2, "A1", 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);
						ExcelRpt_RangeBorder (worksheetHandlesave2, "B1", 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);
						ExcelRpt_RangeBorder (worksheetHandlesave2, "C1", 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);
						ExcelRpt_RangeBorder (worksheetHandlesave2, "D1", 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);
						ExcelRpt_RangeBorder (worksheetHandlesave2, "E1", 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);
						ExcelRpt_RangeBorder (worksheetHandlesave2, "F1", 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);
						ExcelRpt_RangeBorder (worksheetHandlesave2, "G1", 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);
						ExcelRpt_RangeBorder (worksheetHandlesave2, "H1", 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);

						// Nom de la première ligne excel
						ExcelRpt_SetCellValue (worksheetHandlesave2,"A1" , CAVT_CSTRING,"Command Check");
						ExcelRpt_SetCellValue (worksheetHandlesave2,"B1" , CAVT_CSTRING,"FC Value");
						ExcelRpt_SetCellValue (worksheetHandlesave2,"C1" , CAVT_CSTRING,"Labels");
						ExcelRpt_SetCellValue (worksheetHandlesave2,"D1" , CAVT_CSTRING,"Interface");
						ExcelRpt_SetCellValue (worksheetHandlesave2,"E1" , CAVT_CSTRING,"Parameters");
						ExcelRpt_SetCellValue (worksheetHandlesave2,"F1" , CAVT_CSTRING,"Value");
						ExcelRpt_SetCellValue (worksheetHandlesave2,"G1" , CAVT_CSTRING,"Tolerence");
						ExcelRpt_SetCellValue (worksheetHandlesave2,"H1" , CAVT_CSTRING,"Tolerence%");

						//Nb lignes
						ExcelRpt_SetCellValue (worksheetHandlesave2,"I1" , CAVT_INT,*PointeurNbRowsExpR+1);

						// Enregistrement sur le tableau excel par ligne
						for( boucle2 = 1; boucle2 <= *PointeurNbRowsExpR; boucle2++)
						{
							//printf("i am in exp loop\n");
							cell.y = boucle2;
							cell.x =1;
							sprintf(range,"A%d",boucle2+1);
							GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
							ExcelRpt_SetCellValue (worksheetHandlesave2, range, CAVT_CSTRING,sCellValue);
							ExcelRpt_RangeBorder (worksheetHandlesave2, range, 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);

							cell.x =2;
							sprintf(range,"B%d",boucle2+1);
							GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
							ExcelRpt_SetCellValue (worksheetHandlesave2,range , CAVT_CSTRING,sCellValue);
							ExcelRpt_RangeBorder (worksheetHandlesave2, range, 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);

							cell.x =3;
							sprintf(range,"C%d",boucle2+1);
							GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
							ExcelRpt_SetCellValue (worksheetHandlesave2,range , CAVT_CSTRING,sCellValue);
							ExcelRpt_RangeBorder (worksheetHandlesave2, range, 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);

							cell.x =4;
							sprintf(range,"D%d",boucle2+1);
							GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
							ExcelRpt_SetCellValue (worksheetHandlesave2,range , CAVT_CSTRING,sCellValue);
							ExcelRpt_RangeBorder (worksheetHandlesave2, range, 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);

							cell.x =5;
							sprintf(range,"E%d",boucle2+1);   //D2
							GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
							ExcelRpt_SetCellValue (worksheetHandlesave2,range , CAVT_CSTRING, sCellValue);
							ExcelRpt_RangeBorder (worksheetHandlesave2, range, 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);

							cell.x =6;
							sprintf(range,"F%d",boucle2+1);
							GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
							ExcelRpt_SetCellValue (worksheetHandlesave2,range ,  CAVT_CSTRING, sCellValue);
							ExcelRpt_RangeBorder (worksheetHandlesave2, range, 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);

							cell.x =7;
							sprintf(range,"G%d",boucle2+1);
							GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
							ExcelRpt_SetCellValue (worksheetHandlesave2,range ,  CAVT_CSTRING, sCellValue);
							ExcelRpt_RangeBorder (worksheetHandlesave2, range, 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);

							cell.x =8;
							sprintf(range,"H%d",boucle2+1);
							GetTableCellVal(GiPanel,PANEL_MODE_EXP_RESULTS,cell, sCellValue);
							ExcelRpt_SetCellValue (worksheetHandlesave2,range ,  CAVT_CSTRING, sCellValue);
							ExcelRpt_RangeBorder (worksheetHandlesave2, range, 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);
						}   //for

						//D:\Carolina\Test_12.2_WU_TestBenchAutoTool(LC)_Amelioration_Copie_carolina\

						//FileSelectPopup(defaultDirectoryER,"*.xlsx", "*.xlsx", "Save files as...", VAL_SAVE_BUTTON, 0, 1, 1, 1, defaultDirectoryER);
						FileSelectPopupEx (defaultDirectoryER, "*.xlsx", "*.xlsx", "Save files as...", VAL_SAVE_BUTTON, 0, 1, defaultDirectoryER);
						// Fermeture excel
						ExcelRpt_WorkbookSave(workbookHandlesave, defaultDirectoryER, ExRConst_DefaultFileFormat);
						ExcelRpt_WorkbookClose (workbookHandlesave, 0);
										
						SetCtrlVal(GiPanel, PANEL_MODE_ENDTIME, reset_endtime);

					}

				}

				//save file
				//ExcelRpt_ApplicationQuit(applicationHandlesave);
				//CA_DiscardObjHandle(applicationHandlesave);
				//printf("i am here \n");
				// fermeture d'excel
				ExcelRpt_WorkbookClose (workbookHandle8, 0);
				//ExcelRpt_ApplicationQuit (applicationHandle8);
				//CA_DiscardObjHandle(applicationHandle8);

				//SetCtrlAttribute (GiPanel, PANEL_MODE_SAVEFILEBUTTON, ATTR_CMD_BUTTON_COLOR , 0x00F0F0F0);
				SetCtrlVal (GiPanel, PANEL_MODE_SAVEFILEBUTTON, 0);

			}

			break;

	}


	return 0;
}

//***********************************************************************************************



//*****************************************************************************************************
//*****************************************************************************************************
// MODIF MaximePAGES 23/06/2020 - Ajout du code permettant d'éviter une surcharge de mémoire par les proc EXCEL.EXE

// Definition structure d'un élément, de la liste chainée et fonctions associées


typedef struct Element Element;
struct Element
{
	int number;
	Element *next;
};

typedef struct List List;
struct List
{
	Element *first;
};

List *initialisationList()
{
	List *list = malloc(sizeof(*list));
	Element *element = malloc(sizeof(*element));

	if (list == NULL || element == NULL)
	{
		exit(EXIT_FAILURE);
	}

	element->number = 0;
	element->next = NULL;
	list->first = element;

	return list;
}

void insertElement(List *list, int newNumber)
{
	/* Creation of the new element */
	Element *new = malloc(sizeof(*new));
	if (list == NULL || new == NULL)
	{
		exit(EXIT_FAILURE);
	}
	new->number = newNumber;

	/* Insertion of the element at the beginning of the list */
	new->next = list->first;
	list->first = new;
}


/* we remove the first element of the list */
void removeFirstElement(List *list)
{
	if (list == NULL)
	{
		exit(EXIT_FAILURE);
	}

	if (list->first != NULL)
	{
		Element *toRemove = list->first;
		list->first = list->first->next;
		free(toRemove);
	}
}

/* we remove all elements of the list */
void removeAllElements(List *list)
{
	if (list == NULL)
	{
		exit(EXIT_FAILURE);
	}

	while (list->first != NULL)
	{
		Element *toRemove = list->first;
		list->first = list->first->next;
		free(toRemove);
	}

}

/* we kill all proc with PID listed  */
void killAllProcExcel(List *listPIDexcel)
{

	char mycmd[50] = "";


	if (listPIDexcel == NULL)
	{
		exit(EXIT_FAILURE);
	}

	while (listPIDexcel->first != NULL)
	{
		Element *toRemove = listPIDexcel->first;
		listPIDexcel->first = listPIDexcel->first->next;

		sprintf(mycmd,"taskkill /PID \"%d\" /F",toRemove->number);
		WinExec(mycmd,SW_HIDE);
		free(toRemove);
	}

}

//MARVYN 09/07/2021 
int PathSpaces(char *string)
{
	char *aux="#";
	strcat(aux,string);
	strcpy(string,aux);
	string[strlen(string)]='#';
	
	return 0;
}



/*//MARVYN 09/07/2021
int replaceSpaces(char *string)
{
	int lenght = strlen(string);
	char aux[500];
	//µprintf("string[0] = %c",string[0]);
	for (int i=0;i<lenght;i++)
	{
		//printf("string = %s\n",string);
		if (string[i]==' ')
		{
		  if ( i == lenght-1)
		  {
			  //printf("we are in the else");
			  string[i]=NULL;
		  }
		  else
		  {
			  //printf("strlen = %d\n",lenght-i);
			  for (int y=0;y<lenght-i;y++)
			  {
			//	  printf("strlen = %d\n",lenght-i-1);
			//	  printf("y = %d",y);
			//	  printf("\n string = %c\n",string[i+1+y]);
				  aux[y]=string[i+1+y];
			  }
			aux[y+1]='\0';
			string[i]='\\';
			string[i+1]=' ';
			string[i+2]=NULL;
			//printf("string = %s\n",string);
			///printf("aux = %s\n",aux);
			strcat(string,aux);
			printf("string = %s\n",string);
			lenght = strlen(string);
			i++;
		  }
		}
	}
	return 0;
} */

// functions that manage PIDs process
int returnLastExcelPID()
{

	char myfilepath[MAX_PATHNAME_LEN];
	FILE* fichier = NULL;
	int excelPID = 0;
	char derniere[50]="";
	char mycmd[250]="cmd.exe /c TASKLIST /FI \"imagename eq EXCEL.EXE\"  /nh /fo csv  > ";


	GetProjectDir(myfilepath);
	strcat(myfilepath,"\\ExcelTasksPID.txt"); //we define  the name of the file
	//PathSpaces(myfilepath);
	//printf("path = %s\n",myfilepath);
	strcat(mycmd,myfilepath);
	//printf("cmd = %s\n",mycmd);
	WinExec(mycmd, SW_HIDE); //we create a txt file with the list of PID excel process
	Sleep(500);  // we wait 500ms to acquire the information from the file



	fichier = fopen(myfilepath, "r");

	if (fichier != NULL)
	{

		while(fgets(derniere, 50, fichier) != NULL) {} // we position the cursor on the last line

		sscanf(derniere, "\"EXCEL.EXE\",\"%d\"",&excelPID); //we want the pid of the last excel process (end line)

		fclose(fichier);
	}
	return excelPID;
}


void killPIDprocess(int pid)
{
	char mycmd[150] = "";
	sprintf(mycmd,"cmd.exe /c taskkill /F /PID %d",pid);
	//printf("mycmd = %s",mycmd);
	WinExec(mycmd, SW_HIDE);

}

//*****************************************************************************************************


//*****************************************************************************************************
// MODIF MaximePAGES 03/07/2020 - E205

int CVICALLBACK btntolval (int panel, int control, int event,
						   void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{
		case EVENT_COMMIT:

			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE1,		ATTR_DIMMED,1);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE2,		ATTR_DIMMED,0);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_BTNTOLPRCT,0);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_BTNTOLVAL,1);
			
			dimmedNumericKeypadForExp(0);
			currentTxtBox = 12;
			

			break;
	}
	return 0;
}

int CVICALLBACK btntolprct (int panel, int control, int event,
							void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{
		case EVENT_COMMIT:

			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE1,		ATTR_DIMMED,0);
			SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TOLERENCE2,		ATTR_DIMMED,1);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_BTNTOLVAL,0);
			SetCtrlVal(GiExpectedResultsPanel,EXPRESULTS_BTNTOLPRCT,1);
			
			dimmedNumericKeypadForExp(0);
			currentTxtBox = 13;
			

			break;
	}
	return 0;
}


//*****************************************************************************************************








//*****************************************************************************************************


int CVICALLBACK butDebug1 (int panel, int control, int event,
						   void *callbackData, int eventData1, int eventData2)
{

	char sProjectDir[MAX_PATHNAME_LEN];
	char ProjectDirectory[MAX_PATHNAME_LEN];
	char *confpath;
	char fileName[MAX_PATHNAME_LEN];
	int iRet=0;

	switch (event)
	{
		case EVENT_COMMIT:

			GetProjectDir(sProjectDir);
			strcpy(ProjectDirectory, sProjectDir);
			confpath = strcat(ProjectDirectory,"\\Config");

			iRet = FileSelectPopup (confpath, "*.","txt", "", VAL_LOAD_BUTTON, 0, 0, 1, 1, fileName);
			if(iRet != 0) LoadConfiguration(fileName);




			break;
	}
	return 0;
}

int CVICALLBACK butDebug2 (int panel, int control, int event,
						   void *callbackData, int eventData1, int eventData2)
{


	switch (event)
	{
		case EVENT_COMMIT:




			break;
	}
	return 0;
}


void printCheckResults (FILE * file_handle, char * functionName, int valueReturned, nbOfValidData, nbOfInvalidData, nbOfFrames, nbOfBursts, userValue,double average, double max, double min)
{

	char auxStr[25] = "";
	
	// CheckTimingInterFrames
	if(strcmp(functionName, "CheckTimingInterFrames") == 0)
	{
		fprintf(file_handle, "\n");
		fprintf(file_handle, "================================================\n");
		fprintf(file_handle, "|            %s            |\n",functionName);
		fprintf(file_handle, "|                                              |\n");
		fprintf(file_handle, "| %-38s %5d |\n","Frames",nbOfFrames);
		fprintf(file_handle, "| %-38s %5d |\n","Bursts",nbOfBursts);
		fprintf(file_handle, "| %-38s %5d |\n","Valid IF",nbOfValidData);
		fprintf(file_handle, "| %-38s %5d |\n","Invalid IF",nbOfInvalidData);
		fprintf(file_handle, "|                                              |\n");

		if(valueReturned == 1 || valueReturned == -1)
		{
			iCheckTimingInterFrames = 1;
			contErr++;
			fprintf(file_handle, "|                FAILED %3d/%3d                |\n",nbOfInvalidData,(nbOfInvalidData+nbOfValidData));
		}
		else
		{
			fprintf(file_handle, "|                PASSED %3d/%3d                |\n",nbOfValidData,(nbOfInvalidData+nbOfValidData));
		}
		fprintf(file_handle, "================================================\n\n");
	}


	// CheckTimingInterBursts
	if(strcmp(functionName, "CheckTimingInterBursts") == 0)
	{
		fprintf(file_handle, "\n");
		fprintf(file_handle, "================================================\n");
		fprintf(file_handle, "|            %s            |\n",functionName);
		fprintf(file_handle, "|                                              |\n");
		fprintf(file_handle, "| %-38s %5d |\n","Frames",nbOfFrames);
		fprintf(file_handle, "| %-38s %5d |\n","Bursts",nbOfBursts);
		fprintf(file_handle, "| %-38s %5d |\n","Valid IB",nbOfValidData);
		fprintf(file_handle, "| %-38s %5d |\n","Invalid IB",nbOfInvalidData);
		fprintf(file_handle, "|                                              |\n");

		if(valueReturned == 1 || valueReturned == -1)
		{
			iCheckTimingInterBursts = 1;
			contErr++;
			fprintf(file_handle, "|                FAILED %3d/%3d                |\n",nbOfInvalidData,(nbOfInvalidData+nbOfValidData));
		}
		else
		{
			fprintf(file_handle, "|                PASSED %3d/%3d                |\n",nbOfValidData,(nbOfInvalidData+nbOfValidData));
		}
		fprintf(file_handle, "================================================\n\n");
	}


	// CheckNbBursts
	if(strcmp(functionName, "CheckNbBursts") == 0)
	{
		fprintf(file_handle, "\n");
		fprintf(file_handle, "================================================\n");
		fprintf(file_handle, "|                %s                 |\n",functionName);
		fprintf(file_handle, "|                                              |\n");
		fprintf(file_handle, "| %-38s %5d |\n","Bursts",nbOfBursts);
		fprintf(file_handle, "|                                              |\n");

		if(valueReturned == 1 || valueReturned == -1)
		{
			iCheckNbofBurst = 1;
			contErr++;
			fprintf(file_handle, "|                FAILED %3d/%3d                |\n",nbOfBursts,userValue);
		}
		else
		{
			fprintf(file_handle, "|                PASSED %3d/%3d                |\n",nbOfBursts,userValue);
		}
		fprintf(file_handle, "================================================\n\n");
	}


	// CheckNbFramesInBurst
	if(strcmp(functionName, "CheckNbFramesInBurst") == 0)
	{
		fprintf(file_handle, "\n");
		fprintf(file_handle, "================================================\n");
		fprintf(file_handle, "|            %s              |\n",functionName);
		fprintf(file_handle, "|                                              |\n");
		fprintf(file_handle, "| %-38s %5d |\n","Frames in first burst",nbOfFrames);
		fprintf(file_handle, "|                                              |\n");

		if(valueReturned == 1 || valueReturned == -1)
		{
			iCheckNbFramesInBurst = 1;
			contErr++;
			fprintf(file_handle, "|                FAILED %3d/%3d                |\n",nbOfFrames,userValue);
		}
		else
		{
			fprintf(file_handle, "|                PASSED %3d/%3d                |\n",nbOfFrames,userValue);
		}
		fprintf(file_handle, "================================================\n\n");
	}


	// CheckNoRF
	if(strcmp(functionName, "CheckNoRF") == 0)
	{
		fprintf(file_handle, "\n");
		fprintf(file_handle, "================================================\n");
		fprintf(file_handle, "|                   %s                  |\n",functionName);
		fprintf(file_handle, "|                                              |\n");

		if(valueReturned == 1 || valueReturned == -1)
		{
			iCheckNoRF = 1;
			contErr++;
			fprintf(file_handle, "|               FAILED %3d frames              |\n",nbOfFrames);
		}
		else
		{
			fprintf(file_handle, "|                    PASSED                    |\n");
		}
		fprintf(file_handle, "================================================\n\n");
	}


	//  CheckTimingFirstRF
	if(strcmp(functionName, "CheckTimingFirstRF") == 0)
	{
		fprintf(file_handle, "\n");
		fprintf(file_handle, "================================================\n");
		fprintf(file_handle, "|               %s             |\n",functionName);
		fprintf(file_handle, "|                                              |\n");

		if(valueReturned == 1 || valueReturned == -1)
		{
			iCheckTimingFirstRF = 1;
			contErr++;
			fprintf(file_handle, "|                    FAILED                    |\n");    //here,nbOfValidData represents the time of the first RF.
		}
		else
		{
			fprintf(file_handle, "|                    PASSED                    |\n");
		}
		fprintf(file_handle, "================================================\n\n");
	}


	// CheckFieldValue
	if(strcmp(functionName, "CheckFieldValue") == 0)
	{
		fprintf(file_handle, "\n");
		fprintf(file_handle, "================================================\n");
		fprintf(file_handle, "|               %s                |\n",functionName);
		fprintf(file_handle, "|                                              |\n");
		fprintf(file_handle, "| %-38s %5d |\n","Frames with valid value",nbOfValidData);
		fprintf(file_handle, "| %-38s %5d |\n","Frames with invalid value",nbOfInvalidData);
		fprintf(file_handle, "|                                              |\n");

		if(valueReturned == 1 || valueReturned == -1)
		{
			iResultCheckFieldValue = 1;
			contErr++;
			fprintf(file_handle, "|                FAILED %3d/%3d                |\n",nbOfInvalidData,(nbOfInvalidData+nbOfValidData));
		}
		else
		{
			fprintf(file_handle, "|                PASSED %3d/%3d                |\n",nbOfValidData,(nbOfInvalidData+nbOfValidData));
		}
		fprintf(file_handle, "================================================\n\n");
	}


	// CheckCompareP
	if(strcmp(functionName, "CheckCompareP") == 0)
	{
		fprintf(file_handle, "\n");
		fprintf(file_handle, "================================================\n");
		fprintf(file_handle, "|                %s                 |\n",functionName);
		fprintf(file_handle, "|                                              |\n");
		fprintf(file_handle, "| %-38s %5d |\n","Valid Pressure",nbOfValidData);
		fprintf(file_handle, "| %-38s %5d |\n","Invalid Pressure",nbOfInvalidData);
		fprintf(file_handle, "|                                              |\n");

		if(valueReturned == 1 || valueReturned == -1)
		{
			iCheckCompareP = 1;
			contErr++;
			fprintf(file_handle, "|                FAILED %3d/%3d                |\n",nbOfInvalidData,(nbOfInvalidData+nbOfValidData));
		}
		else
		{
			fprintf(file_handle, "|                PASSED %3d/%3d                |\n",nbOfValidData,(nbOfInvalidData+nbOfValidData));
		}
		fprintf(file_handle, "================================================\n\n");
	}


	// CheckCompareAcc
	if(strcmp(functionName, "CheckCompareAcc") == 0)
	{
		fprintf(file_handle, "\n");
		fprintf(file_handle, "================================================\n");
		fprintf(file_handle, "|                %s               |\n",functionName);
		fprintf(file_handle, "|                                              |\n");
		fprintf(file_handle, "| %-38s %5d |\n","Valid Acceleration",nbOfValidData);
		fprintf(file_handle, "| %-38s %5d |\n","Invalid Acceleration",nbOfInvalidData);
		fprintf(file_handle, "|                                              |\n");

		if(valueReturned == 1 || valueReturned == -1)
		{
			iCheckCompareAcc = 1;
			contErr++;
			fprintf(file_handle, "|                FAILED %3d/%3d                |\n",nbOfInvalidData,(nbOfInvalidData+nbOfValidData));
		}
		else
		{
			fprintf(file_handle, "|                PASSED %3d/%3d                |\n",nbOfValidData,(nbOfInvalidData+nbOfValidData));
		}
		fprintf(file_handle, "================================================\n\n");
	}
	
	// CheckAverage
	if(strcmp(functionName, "CheckAverage") == 0)
	{
		fprintf(file_handle, "\n");
		fprintf(file_handle, "================================================\n");
		fprintf(file_handle, "|                  %s                |\n",functionName);
		fprintf(file_handle, "|                                              |\n");
		
		
		if(valueReturned == -1) 
		{
			iCheckAverage = 1;
			contErr++;
			fprintf(file_handle, "|                   FAILED                     |\n");
		}
		else 
		{
			sprintf(auxStr,"%5.3f",average);
			fprintf(file_handle, "| %-35s %8s |\n","The Avarage of the values is:",auxStr);
			sprintf(auxStr,"%5.3f",max); 
			fprintf(file_handle, "| %-35s %8s |\n","The max value between labels is:",auxStr);
			sprintf(auxStr,"%5.3f",min); 
			fprintf(file_handle, "| %-35s %8s |\n","The min value between labels is:",auxStr);
			fprintf(file_handle, "|                                              |\n");
			fprintf(file_handle, "|                   PASSED                     |\n");
		}
		fprintf(file_handle, "================================================\n\n");
	}
	
	// CheckSTDEV
	if(strcmp(functionName, "CheckSTDEV") == 0)
	{
		fprintf(file_handle, "\n");
		fprintf(file_handle, "================================================\n");
		fprintf(file_handle, "|                  %s                  |\n",functionName);
		fprintf(file_handle, "|                                              |\n");
		sprintf(auxStr,"%5.3f",average); 
		fprintf(file_handle, "| %-35s %8s |\n","The Standard deviation * 3 is:",auxStr);
		fprintf(file_handle, "|                                              |\n");
		
		//CheckPreSTDEV
		if (nbOfValidData == 0)
		{
			fprintf(file_handle, "| %-35s %8s |\n","Check Pre-STDEV...","PASSED"); 	
		}
		else 
		{
			fprintf(file_handle, "| %-35s %8s |\n","Check Pre-STDEV...","FAILED"); 
		}
		
		//CheckIndivSTDEV
		if (nbOfInvalidData == 0)
		{
			fprintf(file_handle, "| %-35s %8s |\n","Check Individual STDEV...","PASSED");
		}
		else
		{
			fprintf(file_handle, "| %-35s %8s |\n","Check Individual STDEV...","FAILED"); 
		}
		
		//CheckSTDEV
		if (valueReturned == 0)
		{
			fprintf(file_handle, "| %-35s %8s |\n","Check Global STDEV...","PASSED");
		}
		else
		{
			fprintf(file_handle, "| %-35s %8s |\n","Check Global STDEV...","FAILED"); 
		}
		
		
		fprintf(file_handle, "|                                              |\n"); 
		if(valueReturned != 0 || nbOfValidData != 0) // || nbOfInvalidData != 0)
		{
			iResultCheckSTDEV = 1;
			contErr++;
			fprintf(file_handle, 	"|                    FAILED                    |\n");
		}
		else
		{
			fprintf(file_handle, 	"|                    PASSED                    |\n");
		}
		fprintf(file_handle, "================================================\n\n");
	}
	
	
}

		 
int CheckCompareAcc(char * wuID, char * value, double dtolValue, double dtolpercent, char *FCParam, char* InterfacetoCheck)
{
	int boucle = 0, end = 1, endCondition = 1, zerofound = 0, CheckCAcc = 0, i=0, valide = 0, notvalide =0;
	char CheckGoodsum[15] = 	"/ROW/@";
	char CheckRTime[15] = 		"/ROW/@";
	char Rowparam2[255] = 		"/ROW/@";  //for FC 
	char CheckAcc[30] = 		"/ROW/@";
	char CheckWUID[30] = 		"/ROW/@"; 
	char CKSGood[3] = "";
	char _AccGTimeColumn[255] = "";
	char _ATimeColumn[255] = "";
	char _RTimeColumn[255] = "";
	char _AccColumn[255] = "";
	char _ASpeedColumn[255] = "";
	char _WUIDColumn[255] = ""; 
	char rangeA[30] = "";
	char RTimeRange[30]="";
	char GoodSumRange[30] = "";
	
	char AccRange[30] = "";
	char ASpeedRange[30] = "";
	char WUIDRange[30] = ""; 
	char foundCellGoodSumRange[30] = "";
	char cellRTimeRange[30]="";
	char cellAccGRange[30]="";
	char cellAccRange[30]="";
	char cellGoodSumRange[30] = "";
	char cellWUIDRange[30] = "";  
	char RTime[30] = "";
	char AccG[30] = "";
	char WUID[30] = ""; 
	char *CheckParamFC; 
	char Acc[30] = "";
	int noLab=0;
	
	int m=1;

	double dValueMax=0.0, dValueMin = 0.0, dValueMaxperc = 0.0, dValueMinperc = 0.0;
	char percent[4]="[%]";

	double postLSEAcceleration_g = 0.0;
	double preLSEAcceleration_g = 0.0;

	//AJOUT MaximePAGES


	double RFRTime = 0.0;
	double RFAcc = 0.0;
	double RFASpeed = 0.0;
	double RFAcc_g = 0.0;
	
	char cellDescRange[30]="";
	char Desc[30] = "";
	char DescRange[30] = "";
	char _DescColumn[255] = "";
	char CheckDesc[15] = 	"/ROW/@Desc"; //?
	
	double wheeldim = 0;
	double m_pi = 3.14159265358;

	int CKSGoodvalid = 0;
	int CKSGoodinvalid = 0;

	//AJOUT MARVYN
	double postLSERTime = 0.0;
	double preLSERTime = 0.0;
	double postLSEAcceleration= 0.0;
	double preLSEAcceleration = 0.0;

	
	double RFAcceleration = 0.0;
	double RFAccelerationInterpol = 0.0;

	int postLSEFound = 0;
	int preLSEFound = 0;
	 
	
	strcat(CheckAcc,translateData(InterfacetoCheck, "RotSpeed"));   
	strcat(CheckRTime,translateData(InterfacetoCheck, "Time")); 
	strcat(CheckGoodsum,translateData(InterfacetoCheck, "CKS_Validity"));
	strcat(CheckWUID,translateData(InterfacetoCheck,"ID"));
	//printf("CheckWUID = %s\n",CheckWUID);
	//printf("CheckRTime = %s\n",CheckRTime);
	//printf("CheckGoodsum = %s\n",CheckGoodsum); 
	

	
	CheckParamFC = strcat(Rowparam2, FCParam);
	
	fprintf(file_handle, "\n***************CheckCompareAcc***************\n");
	fprintf(file_handle, "                 Check_%d\n\n", NBTestCheck);
	//printf("I AM HERE !");

		//Worksheet Logs
	ExcelRpt_GetWorksheetFromIndex(workbookHandle9, 1, &worksheetHandle9);

	

	
	// 1 - looking for "/ROW/@CKSGood"
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckGoodsum, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,foundCellGoodSumRange);
	char caracter1[] = "$";  //foundCellRange =  $O$2
	char *token1 = strtok(foundCellGoodSumRange, caracter1);   //token1 = O
	//printf("token = %s\n",token1);
	if(token1 == NULL)
	{
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckGoodsum);
	}
	else
		strcpy(cellGoodSumRange, token1);


	//2 - looking for "/ROW/@Acceleration" = $C$2 in the frame, from the LSE (opt)
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckAcc, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,Acc);
	char caracter2[] = "$";
	char *token2 = strtok(Acc, caracter2);
	if(token2 == NULL) {
		//printf("I AM HERE 2!"); 
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckAcc);
	}
	else
		strcpy(cellAccRange, token2);


	//3 - looking for "/ROW/@_RTime" = $B$2
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckRTime, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,RTime);
	char caracter3[] = "$";
	char *token3 = strtok(RTime, caracter3);
	if(token3 == NULL)
	{	
		//printf("I AM HERE 3!"); 
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckRTime);
	}	
	else
		strcpy(cellRTimeRange, token3); // cellFCRange = B2


	
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, Rowparam2, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,AccG);
	char caracter4[] = "$";
	char *token4 = strtok(AccG, caracter4);
	if(token4 == NULL)
	{
		//printf("I AM HERE 4!"); 
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", Rowparam2);
	}
	else
		strcpy(cellAccGRange, token4);
	
	//5 - looking for "/ROW/@Desc" = $S$2
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckDesc, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,Desc);
	char caracter5[] = "$";
	char *token5 = strtok(Desc, caracter5);
	if(token5 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckDesc);
	else
		strcpy(cellDescRange, token5);
	
	
	//6 - looking for "/ROW/@ID" = $W$2 // the WU ID
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckWUID, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,WUID);
	char caracterWUID[] = "$";
	char *tokenWUID = strtok(WUID, caracterWUID);
	if(tokenWUID == NULL)
	{
		//printf("I AM HERE 5!"); 
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckWUID);
	}
	else
		strcpy(cellWUIDRange, tokenWUID);
	

	//wheeldim from excel
	wheeldim = atof(value); //wheeldim = traslateParameterValue(value); MODIF MARVYN 21/07/2021
	//printf("wheeldim =%f\n",wheeldim);
	
	if(token1 != NULL && token2 != NULL && token3 != NULL && token4 != NULL && tokenWUID != NULL)
	{
			if (lab2 == 0)
		{
			if (lab1 == 0)
			{
			fprintf(file_handle, "Between %s (%d ms) and %s (%d ms)\n", 	label1, Time1, label2, Time2);
			}
			else
			{
			fprintf(file_handle, "Between the beginning and %s (%d ms)\n", label2, Time2);
			}
		}
		else
		{
		   	if (lab1 == 0)
			{
			fprintf(file_handle, "Between %s (%d ms) and the end of the script\n", 	label1, Time1);
			}
			else
			{
			fprintf(file_handle, "Between the beginning and the end of the script\n");
			}
		}
		fprintf(file_handle, "Tolerance value: %f\n", 	dtolValue);
		fprintf(file_handle, "Tolerance %s: %f\n\n",	percent, dtolpercent);

		for(boucle=2; end==1; boucle++)
		{
			//until find label1
			sprintf(rangeA, "A%d", boucle+1);
			ExcelRpt_GetCellValue (worksheetHandle9, rangeA, CAVT_CSTRING, _ATimeColumn);

			//starting check when label1 was found
			if(strcmp(label1, _ATimeColumn) == 0 || lab1==1)
			{
				//analyse until find label 2
				for(i = boucle+1 ; endCondition == 1; i++)
				{

					//1 : we look for a LSE frame - check Desc = LSE
					sprintf(DescRange,"%s%d",cellDescRange, i);
					//printf("cellDescRange = %s\n",cellDescRange);
					ExcelRpt_GetCellValue (worksheetHandle9, DescRange, CAVT_CSTRING, _DescColumn);
					//printf("DescColumn = %s\n",_DescColumn);
					if(strcmp(_DescColumn, "Optical") == 0)
					{

						//preOptFound
						preLSEFound = 1;

						//save post RTime for opt frame
						sprintf(RTimeRange,"%s%d",cellRTimeRange, i);
						ExcelRpt_GetCellValue (worksheetHandle9, RTimeRange, CAVT_CSTRING, _RTimeColumn);
						preLSERTime = atof(_RTimeColumn);

						//save post Acceleration from LSE
						sprintf(AccRange,"%s%d",cellAccRange, i);
						ExcelRpt_GetCellValue (worksheetHandle9, AccRange, CAVT_CSTRING, _AccColumn);
						preLSEAcceleration = atof(_AccColumn);
						
						//convert ASpeed [rpm] to ASpeed [g] knowing wheel Dim (value)
						preLSEAcceleration_g = ((pow((preLSEAcceleration*2*m_pi)/60,2)*(wheeldim/2)*0.0254)/9.81);

					}


					//2 : we look for a RF frame - check Goodsum
					sprintf(GoodSumRange,"%s%d",cellGoodSumRange, i);
					ExcelRpt_GetCellValue (worksheetHandle9, GoodSumRange, CAVT_CSTRING, CKSGood);

					if(strcmp(CKSGood, "1") == 0)
					{

						// we look for the good WU ID
						sprintf(WUIDRange,"%s%d",cellWUIDRange, i);
						ExcelRpt_GetCellValue (worksheetHandle9, WUIDRange, CAVT_CSTRING, _WUIDColumn);

						if(strcmp(_WUIDColumn, wuID) == 0)
						{

							CKSGoodvalid++ ;


							//save post RTime for RF frame
							sprintf(RTimeRange,"%s%d",cellRTimeRange, i);
							ExcelRpt_GetCellValue (worksheetHandle9, RTimeRange, CAVT_CSTRING, _RTimeColumn);
							RFRTime = atof(_RTimeColumn);


							//if we have the pre druck frame, we look for the next druck frame
							if  (preLSEFound == 1)
							{
								m=1;
								//MARVYN 16/07/2021 bug 
								while ((postLSEFound == 0) && (strcmp(_DescColumn, "") != 0))
								{
									// we look for the post LSE frame - check Desc = LSE
									sprintf(DescRange,"%s%d",cellDescRange, i+m);
									ExcelRpt_GetCellValue (worksheetHandle9, DescRange, CAVT_CSTRING, _DescColumn);

									if(strcmp(_DescColumn, "Optical") == 0)
									{

										//postLSEFound
										postLSEFound = 1;

										//save post RTime for LSE frame
										sprintf(RTimeRange,"%s%d",cellRTimeRange, i+m);
										ExcelRpt_GetCellValue (worksheetHandle9, RTimeRange, CAVT_CSTRING, _RTimeColumn);
										postLSERTime = atof(_RTimeColumn);

										//save post Pressure from LSE(Druck)
										sprintf(AccRange,"%s%d",cellAccRange, i+m);
										ExcelRpt_GetCellValue (worksheetHandle9, AccRange, CAVT_CSTRING, _AccColumn);
										postLSEAcceleration = atof(_AccColumn);
										
										postLSEAcceleration_g = ((pow((postLSEAcceleration*2*m_pi)/60,2)*(wheeldim/2)*0.0254)/9.81);
									}
									m++;
								}
							}

						}
					}

					//3 : if post druck and pre druck found, we can interpolate.
					if (  (postLSEFound ==1) && (preLSEFound ==1) )
					{
						postLSEFound = 0;
						//preDruckFound = 0;
						//printf("postLSERTime = %f and  postLSEPressure = %f\n",postLSERTime,postLSEAcceleration_g);
						//printf("preLSERTime = %f and  preLSEPressure = %f\n",preLSERTime,postLSEAcceleration_g);
						//printf("RFRTime = %f\n",RFRTime);    
						// Interpolation calculation to find RFPressureInterpol
						RFAccelerationInterpol =   ((postLSEAcceleration_g*(postLSERTime - RFRTime) +   postLSEAcceleration_g*(RFRTime - preLSERTime))/(postLSERTime - preLSERTime)) ;


						//Now we compare  RFPressureInterpol and  RFPressure +/- tol

						//save  Pressure from WU
						sprintf(ASpeedRange,"%s%d",cellAccGRange, i);
						//printf("cell = %s%d\n",cellAccGRange, i);
						ExcelRpt_GetCellValue (worksheetHandle9, ASpeedRange, CAVT_CSTRING, _ASpeedColumn);
						RFAcc = atof(_ASpeedColumn);
						//printf("RFAcc = %f\n",RFAcc);

						//We compare RFPressure and RFPressureInterpol

						if(dtolValue != 0 && dtolpercent == 0)
						{
							dValueMax = RFAccelerationInterpol + dtolValue;
							dValueMin = RFAccelerationInterpol - dtolValue;
							if(dValueMin<0)
								dValueMin = 0;

							if(RFAcc >= dValueMin && RFAcc <= dValueMax )
							{
								valide++;
								//fprintf(file_handle, "The pressure %f is  between [%f,%f], (frames nb %d)\n", RFPressure, dValueMin, dValueMax,i);
							}
							else
							{
								CheckCAcc = 1;
								notvalide++;
								fprintf(file_handle, "The acceleration %f is not between [%f,%f], (frames nb %d)\n", RFAcc, dValueMin, dValueMax,i);
							}
						}
						else if(dtolValue == 0 && dtolpercent != 0)
						{
							dValueMaxperc = RFAccelerationInterpol + (RFAccelerationInterpol*(dtolpercent));
							dValueMinperc = RFAccelerationInterpol - (RFAccelerationInterpol*(dtolpercent));
							if(dValueMinperc<0)
								dValueMinperc = 0;

							if(RFAcc >= dValueMinperc && RFAcc <= dValueMaxperc)
							{
								//fprintf(file_handle, "The TimingInterBurst %f is between [%f,%f]\n", sub, dValueMinperc, dValueMaxperc);
								valide++;
								//fprintf(file_handle, "The pressure %f is between [%f,%f], (frames nb %d)\n", RFPressure, dValueMin, dValueMax,i);
							}
							else
							{
								notvalide++;
								CheckCAcc = 1;
								fprintf(file_handle, "The acceleration %f is not between [%f,%f], (frames nb %d)\n", RFAcc, dValueMin, dValueMax,i);
							}

						}
						else
						{
							
							if(RFAccelerationInterpol<0)
								RFAccelerationInterpol = 0;

							if(RFAcc >= RFAccelerationInterpol && RFAcc <= RFAccelerationInterpol)
							{
								//fprintf(file_handle, "The TimingInterBurst %f is between [%f,%f]\n", sub, dValueMinperc, dValueMaxperc);
								valide++;
								//fprintf(file_handle, "The pressure %f is between [%f,%f], (frames nb %d)\n", RFPressure, dValueMin, dValueMax,i);
							}
							else
							{
								notvalide++;
								CheckCAcc = 1;
								fprintf(file_handle, "The acceleration %f is not equal to [%f], (frames nb %d)\n", RFAcc, RFAccelerationInterpol, i);
							}
						}

						postLSERTime = 0.0;
						postLSEAcceleration= 0.0;
						RFRTime = 0.0;
						RFAcc = 0.0;
						RFAccelerationInterpol = 0.0;

					}

					if(strcmp(CKSGood, "0") == 0)
					{
						CKSGoodinvalid++ ;
						zerofound = 1;
						fprintf(file_handle, "CKSGood: 0 was found in line %d\n", i);
					}

					sprintf(rangeA, "A%d", i);
					ExcelRpt_GetCellValue (worksheetHandle9, rangeA, CAVT_CSTRING, _ATimeColumn);

					if(strcmp(label2, _ATimeColumn) == 0)   //stop condition of check
					{
						if (noLab == 0 || FirstLab==1  || TwoLabs==1)
						{
						if(zerofound == 1)
						{
							float percentageCKSGood = ((double)CKSGoodinvalid/( (double)CKSGoodinvalid+ (double)CKSGoodvalid))*100.0;
							fprintf(file_handle, "\n%.2f%% of the CKSGood are equal to 0!\n\n",percentageCKSGood );
						}


						endCondition = 0; //stop ckeck
						end = 0;
						}
						noLab=1;
					}
				}
			}
		}

		end=1;
		if(notvalide > 0)
		{
			CheckCAcc =1;
		}

	}

	else
	{
		fprintf(file_handle, "The CheckCompareAcc was not performed!\n\n");
		CheckCAcc = -1;
	}


	printCheckResults (file_handle, "CheckCompareAcc", CheckCAcc, valide, notvalide, 0 , 0 , 0,0.0,0.0,0.0);
	return CheckCAcc;
}



//***********************************************************************************************
// Modif MaximePAGES 08/07/2020 - CheckCompareP implementation
//***********************************************************************************************
int CheckCompareP(char * wuID, double dtolValue, double dtolpercent, char *Param, char* InterfacetoCheck)
{
	int boucle = 0, end = 1, endCondition = 1, zerofound = 0, CheckCP = 0, i=0, valide = 0, notvalide =0;
	char CheckGoodsum[15] = 	"/ROW/@";
	char CheckRTime[15] = 		"/ROW/@";
	char CheckPressure[15] = 		"/ROW/@";
	char Rowparam2[255] = 		"/ROW/@";  
	char CheckDesc[15] = 	"/ROW/@Desc"; // ?
	char CKSGood[3] = "";
	char _ATimeColumn[255] = "";
	char _DescColumn[255] = "";
	char _RTimeColumn[255] = "";
	char _PressureColumn[255] = "";
	char _FormattedPressureColumn[255] = "";
	char rangeA[30] = "";
	char RTimeRange[30]="";
	char GoodSumRange[30] = "";
	char DescRange[30] = "";
	char PressureRange[30] = "";
	char FormattedPressureRange[30] ="";
	char *CheckParam;
	
	char foundCellGoodSumRange[30] = "";
	char cellRTimeRange[30]="";
	char cellFormattedPressureRange[30]="";
	char cellPressureRange[30]="";
	char cellDescRange[30]="";
	char cellGoodSumRange[30] = "";
	char RTime[30] = "";
	char Pressure[30] = "";
	char Desc[30] = "";
	char formattedPressure[30] = "";
	
	


	double dValueMax=0.0, dValueMin = 0.0, dValueMaxperc = 0.0, dValueMinperc = 0.0;
	char percent[4]="[%]";


	//AJOUT MaximePAGES
	double postDruckRTime = 0.0;
	double preDruckRTime = 0.0;
	double postDruckPressure= 0.0;
	double preDruckPressure = 0.0;

	double RFRTime = 0.0;
	double RFPressure = 0.0;
	double RFPressureInterpol = 0.0;

	int postDruckFound = 0;
	int preDruckFound = 0;

	int CKSGoodvalid = 0;
	int CKSGoodinvalid = 0;
	int noLab=0;

	int m=1;

	char CheckWUID[30] = 		"/ROW/@";
	char _WUIDColumn[255] = "";
	char WUIDRange[30] = "";
	char cellWUIDRange[30] = "";
	char WUID[30] = "";

	//MARVYN 30/07/2021
	strcat(CheckPressure,translateData(InterfacetoCheck, "Pressure"));     
	strcat(CheckRTime,translateData(InterfacetoCheck, "Time")); 
	strcat(CheckGoodsum,translateData(InterfacetoCheck, "CKS_Validity"));
	strcat(CheckWUID,translateData(InterfacetoCheck,"ID"));
	//printf("CheckWUID = %s\n",CheckWUID);
	//printf("CheckRTime = %s\n",CheckRTime);
	//printf("CheckGoodsum = %s\n",CheckGoodsum);
	

	
	
	//printf("Rowparam2 = %s and FCParam = %s\n",Rowparam2,Param);
	CheckParam = strcat(Rowparam2, Param);
	//printf("Rowparam2 = %s and FCParam = %s\n",Rowparam2,Param);

	fprintf(file_handle, "\n***************CheckCompareP***************\n");
	fprintf(file_handle, "                 Check_%d\n\n", NBTestCheck);


	//Worksheet Logs
	ExcelRpt_GetWorksheetFromIndex(workbookHandle9, 1, &worksheetHandle9);

	// 1 - looking for "/ROW/@CKSGood"
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckGoodsum, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,foundCellGoodSumRange);
	char caracter1[] = "$";  //foundCellRange =  $O$2
	char *token1 = strtok(foundCellGoodSumRange, caracter1);   //token1 = O
	if(token1 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckGoodsum);
	else
		strcpy(cellGoodSumRange, token1);

	//2 - looking for "/ROW/@Desc" = $S$2
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckDesc, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,Desc);
	char caracter2[] = "$";
	char *token2 = strtok(Desc, caracter2);
	if(token2 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckDesc);
	else
		strcpy(cellDescRange, token2);


	//3 - looking for "/ROW/@_RTime" = $B$2
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckRTime, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,RTime);
	char caracter3[] = "$";
	char *token3 = strtok(RTime, caracter3);
	if(token3 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckRTime);
	else
		strcpy(cellRTimeRange, token3); // cellFCRange = B2
	
	//4 - looking for "/ROW/@Pressure" = $AC$2 // from  the Druck
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckPressure, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,Pressure);
	char caracter4[] = "$";
	char *token4 = strtok(Pressure, caracter4);
	//printf("pressure = %s\n",Pressure);     
	if(token4 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckPressure);
	else
		strcpy(cellPressureRange, token4);
		//printf("cellpressrange = %s\n",cellPressureRange);
		
	//5 - looking for "/ROW/@FormattedPressure" = $S$2 from the WU  MARVYN PANNETIER 15/07/2021
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckParam, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,formattedPressure);
	char caracter5[] = "$";
	char *token5 = strtok(formattedPressure, caracter5);
		//printf("pressure = %s\n",formattedPressure); 
	if(token5 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckParam);
	else
		strcpy(cellFormattedPressureRange, token5);
	//printf("cellpressrange = %s\n",cellFormattedPressureRange);      

	//6 - looking for "/ROW/@ID" = $W$2 // the WU ID
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckWUID, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,WUID);
	char caracterWUID[] = "$";
	char *tokenWUID = strtok(WUID, caracterWUID);
	if(tokenWUID == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckWUID);
	else
		strcpy(cellWUIDRange, tokenWUID);
	
	// AJOUT Marvyn 13/07/2021
	if(token1 != NULL && token2 != NULL && token3 != NULL && token4 != NULL && tokenWUID != NULL)
	{
		if (lab2 == 0)
		{
			if (lab1 == 0)
			{
			fprintf(file_handle, "Between %s (%d ms) and %s (%d ms)\n", 	label1, Time1, label2, Time2);
			}
			else
			{
			fprintf(file_handle, "Between the beginning and %s (%d ms)\n", label2, Time2);
			}
		}
		else
		{
		   	if (lab1 == 0)
			{
			fprintf(file_handle, "Between %s (%d ms) and the end of the script\n", 	label1, Time1);
			}
			else
			{
			fprintf(file_handle, "Between the beginning and the end of the script\n");
			}
		}
		fprintf(file_handle, "Tolerance value: %f\n", 	dtolValue);
		fprintf(file_handle, "Tolerance %s: %f\n\n",	percent, dtolpercent);


		for(boucle=2; end==1; boucle++)
		{
			//until find label1
			sprintf(rangeA, "A%d", boucle+1);
			ExcelRpt_GetCellValue (worksheetHandle9, rangeA, CAVT_CSTRING, _ATimeColumn);

			//starting check when label1 was found
			if(strcmp(label1, _ATimeColumn) == 0 || lab1==1)
			{
				//analyse until find label 2
				for(i = boucle+1 ; endCondition == 1; i++)
				{

					//1 : we look for a Druck frame - check Desc = Druck
					sprintf(DescRange,"%s%d",cellDescRange, i);
					ExcelRpt_GetCellValue (worksheetHandle9, DescRange, CAVT_CSTRING, _DescColumn);

					if(strcmp(_DescColumn, "Druck") == 0)
					{

						//preOptFound
						preDruckFound = 1;

						//save post RTime for opt frame
						sprintf(RTimeRange,"%s%d",cellRTimeRange, i);
						ExcelRpt_GetCellValue (worksheetHandle9, RTimeRange, CAVT_CSTRING, _RTimeColumn);
						preDruckRTime = atof(_RTimeColumn);

						//save post Pressure from LSE(Druck)
						sprintf(PressureRange,"%s%d",cellPressureRange, i);
						ExcelRpt_GetCellValue (worksheetHandle9, PressureRange, CAVT_CSTRING, _PressureColumn);
						preDruckPressure = atof(_PressureColumn);

					}


					//2 : we look for a RF frame - check Goodsum
					sprintf(GoodSumRange,"%s%d",cellGoodSumRange, i);
					ExcelRpt_GetCellValue (worksheetHandle9, GoodSumRange, CAVT_CSTRING, CKSGood);

					if(strcmp(CKSGood, "1") == 0)
					{

						// we look for the good WU ID
						sprintf(WUIDRange,"%s%d",cellWUIDRange, i);
						ExcelRpt_GetCellValue (worksheetHandle9, WUIDRange, CAVT_CSTRING, _WUIDColumn);

						if(strcmp(_WUIDColumn, wuID) == 0)
						{

							CKSGoodvalid++ ;


							//save post RTime for RF frame
							sprintf(RTimeRange,"%s%d",cellRTimeRange, i);
							ExcelRpt_GetCellValue (worksheetHandle9, RTimeRange, CAVT_CSTRING, _RTimeColumn);
							RFRTime = atof(_RTimeColumn);


							//if we have the pre druck frame, we look for the next druck frame
							if  (preDruckFound == 1)
							{
								m=1;
								//MARVYN 16/07/2021 bug 
								while ((postDruckFound == 0) && (strcmp(_DescColumn, "") != 0))
								{
									// we look for the post Druck frame - check Desc = Druck
									sprintf(DescRange,"%s%d",cellDescRange, i+m);
									ExcelRpt_GetCellValue (worksheetHandle9, DescRange, CAVT_CSTRING, _DescColumn);

									if(strcmp(_DescColumn, "Druck") == 0)
									{

										//postDruckFound
										postDruckFound = 1;

										//save post RTime for druck frame
										sprintf(RTimeRange,"%s%d",cellRTimeRange, i+m);
										ExcelRpt_GetCellValue (worksheetHandle9, RTimeRange, CAVT_CSTRING, _RTimeColumn);
										postDruckRTime = atof(_RTimeColumn);

										//save post Pressure from LSE(Druck)
										sprintf(PressureRange,"%s%d",cellPressureRange, i+m);
										ExcelRpt_GetCellValue (worksheetHandle9, PressureRange, CAVT_CSTRING, _PressureColumn);
										postDruckPressure = atof(_PressureColumn);
									}
									m++;
								}
							}

						}
					}

					//3 : if post druck and pre druck found, we can interpolate.
					if (  (postDruckFound ==1) && (preDruckFound ==1) )
					{
						postDruckFound = 0;
						//preDruckFound = 0;

						// Interpolation calculation to find RFPressureInterpol
						RFPressureInterpol =   ((preDruckPressure*(postDruckRTime - RFRTime) +   postDruckPressure*(RFRTime - preDruckRTime))/(postDruckRTime - preDruckRTime)) ;


						//Now we compare  RFPressureInterpol and  RFPressure +/- tol

						//save  Pressure from WU
						sprintf(FormattedPressureRange,"%s%d",cellFormattedPressureRange, i);
						//printf("cell = %s%d\n",cellFormattedPressureRange, i);
						ExcelRpt_GetCellValue (worksheetHandle9, FormattedPressureRange, CAVT_CSTRING, _FormattedPressureColumn);
						RFPressure = atof(_FormattedPressureColumn);
						//printf("RFPressure = %f\n",RFPressure);

						//We compare RFPressure and RFPressureInterpol

						if(dtolValue != 0 && dtolpercent == 0)
						{
							dValueMax = RFPressureInterpol + dtolValue;
							dValueMin = RFPressureInterpol - dtolValue;
							if(dValueMin<0)
								dValueMin = 0;

							if(RFPressure >= dValueMin && RFPressure <= dValueMax )
							{
								valide++;
								//fprintf(file_handle, "The pressure %f is  between [%f,%f], (frames nb %d)\n", RFPressure, dValueMin, dValueMax,i);
							}
							else
							{
								CheckCP = 1;
								notvalide++;
								fprintf(file_handle, "The pressure %f is not between [%f,%f], (frames nb %d)\n", RFPressure, dValueMin, dValueMax,i);
							}
						}
						else if(dtolValue == 0 && dtolpercent != 0)
						{
							dValueMaxperc = RFPressureInterpol + (RFPressureInterpol*(dtolpercent));
							dValueMinperc = RFPressureInterpol - (RFPressureInterpol*(dtolpercent));
							if(dValueMinperc<0)
								dValueMinperc = 0;

							if(RFPressure >= dValueMinperc && RFPressure <= dValueMaxperc)
							{
								//fprintf(file_handle, "The TimingInterBurst %f is between [%f,%f]\n", sub, dValueMinperc, dValueMaxperc);
								valide++;
								//fprintf(file_handle, "The pressure %f is between [%f,%f], (frames nb %d)\n", RFPressure, dValueMin, dValueMax,i);
							}
							else
							{
								notvalide++;
								CheckCP = 1;
								fprintf(file_handle, "The pressure %f is not between [%f,%f], (frames nb %d)\n", RFPressure, dValueMin, dValueMax,i);
							}

						}
						else
						{
							if(RFPressureInterpol<0)
								RFPressureInterpol = 0;

							if(RFPressure >= RFPressureInterpol && RFPressure <= RFPressureInterpol)
							{
								//fprintf(file_handle, "The TimingInterBurst %f is between [%f,%f]\n", sub, dValueMinperc, dValueMaxperc);
								valide++;
								//fprintf(file_handle, "The pressure %f is between [%f,%f], (frames nb %d)\n", RFPressure, dValueMin, dValueMax,i);
							}
							else
							{
								notvalide++;
								CheckCP = 1;
								fprintf(file_handle, "The pressure %f is not equal to [%f], (frames nb %d)\n", RFPressure, RFPressureInterpol, i);
							}
						}

						postDruckRTime = 0.0;
						postDruckPressure= 0.0;
						RFRTime = 0.0;
						RFPressure = 0.0;
						RFPressureInterpol = 0.0;

					}

					if(strcmp(CKSGood, "0") == 0)
					{
						CKSGoodinvalid++ ;
						zerofound = 1;
						fprintf(file_handle, "CKSGood: 0 was found in line %d\n", i);
					}

					sprintf(rangeA, "A%d", i);
					ExcelRpt_GetCellValue (worksheetHandle9, rangeA, CAVT_CSTRING, _ATimeColumn);

					if(strcmp(label2, _ATimeColumn) == 0)   //stop condition of check
					{
						if (noLab == 0 || FirstLab==1  || TwoLabs==1)
						{
						if(zerofound == 1)
						{
							float percentageCKSGood = ((double)CKSGoodinvalid/( (double)CKSGoodinvalid+ (double)CKSGoodvalid))*100.0;
							fprintf(file_handle, "\n%.2f%% of the CKSGood are equal to 0!\n\n",percentageCKSGood );
						}


						endCondition = 0; //stop ckeck
						end = 0;
						}
						noLab=1;
					}

				}
			}
		}

		end=1;
		if(notvalide > 0)
		{
			CheckCP =1;
		}

	}

	else
	{
		fprintf(file_handle, "The CheckCompareP was not performed!\n\n");
		CheckCP = -1;
	}


	printCheckResults (file_handle, "CheckCompareP", CheckCP, valide, notvalide, 0 , 0 , 0,0.0,0.0,0.0);
	return CheckCP;
}






void generateCoverageMatrix(char * currentDirPath, int currentTestStatus)
{


	char worksheetName[15]="CoverageMatrix";
	char range[30];
	int y = 0;
	char * s = "\\";
	char * token="";
	//char * currentDirName="";
	char currentDirName[255] = {0};


	if (testInSeqCounter == 0)   //première fois qu'on appelle la fonction. On crée l'excel.
	{

		// cration du workbook et du worksheet
		ExcelRpt_WorkbookNew (applicationHandleProject, &workbookHandleCoverageMatrix);
		ExcelRpt_GetWorksheetFromIndex(workbookHandleCoverageMatrix, 1, &worksheetHandleCoverageMatrix);
		ExcelRpt_SetWorksheetAttribute (worksheetHandleCoverageMatrix, ER_WS_ATTR_NAME, worksheetName);


		// Esthétique excel
		ExcelRpt_SetCellRangeAttribute (worksheetHandleCoverageMatrix, "A1:D1", ER_CR_ATTR_FONT_BOLD, 1);
		ExcelRpt_SetCellRangeAttribute (worksheetHandleCoverageMatrix, "A1:D1", ER_CR_ATTR_BGCOLOR, 0xC0C0C0L);
		ExcelRpt_SetCellRangeAttribute (worksheetHandleCoverageMatrix, "A1:D1", ER_CR_ATTR_HORIZ_ALIGN, ExRConst_HAlignCenter);

		ExcelRpt_SetCellRangeAttribute (worksheetHandleCoverageMatrix, "A2:B100", ER_CR_ATTR_HORIZ_ALIGN, ExRConst_HAlignCenter);
		ExcelRpt_SetCellRangeAttribute (worksheetHandleCoverageMatrix, "C2:C100", ER_CR_ATTR_HORIZ_ALIGN, ExRConst_HAlignLeft);
		ExcelRpt_SetCellRangeAttribute (worksheetHandleCoverageMatrix, "D2:D100", ER_CR_ATTR_HORIZ_ALIGN, ExRConst_HAlignCenter);

		ExcelRpt_RangeBorder (worksheetHandleCoverageMatrix, "A1", 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);
		ExcelRpt_RangeBorder (worksheetHandleCoverageMatrix, "B1", 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);
		ExcelRpt_RangeBorder (worksheetHandleCoverageMatrix, "C1", 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);
		ExcelRpt_RangeBorder (worksheetHandleCoverageMatrix, "D1", 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);

		//Largeur des cell
		ExcelRpt_SetCellRangeAttribute (worksheetHandleCoverageMatrix, "A1", ER_CR_ATTR_COLUMN_WIDTH, 50.0);
		ExcelRpt_SetCellRangeAttribute (worksheetHandleCoverageMatrix, "B1", ER_CR_ATTR_COLUMN_WIDTH, 14.0);
		ExcelRpt_SetCellRangeAttribute (worksheetHandleCoverageMatrix, "C1", ER_CR_ATTR_COLUMN_WIDTH, 20.0);
		ExcelRpt_SetCellRangeAttribute (worksheetHandleCoverageMatrix, "D1", ER_CR_ATTR_COLUMN_WIDTH, 20.0);



		// Nom de la première ligne excel
		ExcelRpt_SetCellValue (worksheetHandleCoverageMatrix,"A1" , CAVT_CSTRING,"Test Name");
		ExcelRpt_SetCellValue (worksheetHandleCoverageMatrix,"B1" , CAVT_CSTRING,"Status");
		ExcelRpt_SetCellValue (worksheetHandleCoverageMatrix,"C1" , CAVT_CSTRING,"Directory Path");
		ExcelRpt_SetCellValue (worksheetHandleCoverageMatrix,"D1" , CAVT_CSTRING,"Check_Results.txt");

	}
	else
	{
		// on récupère le nom du dossier de test en cours

		/* get the first token */

		strcpy(currentDirName,currentDirPath);
		token = strtok(currentDirName, s);

		/* walk through other tokens */
		while( token != NULL )
		{
			strcpy(currentDirName,token);
			token = strtok(NULL, s);
		}


		y=testInSeqCounter+1; //ex : si 1er test, alors on écrit à la deuxième ligne.
		
		//colonne Test Name
		sprintf(range,"A%d",y);
		ExcelRpt_SetCellValue (worksheetHandleCoverageMatrix, range, CAVT_CSTRING,currentDirName);
		//ExcelRpt_SetCellValue (worksheetHandleCoverageMatrix, range, CAVT_CSTRING,"oui");
		ExcelRpt_RangeBorder (worksheetHandleCoverageMatrix, range, 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);


		//colonne Directory Path
		sprintf(range,"C%d",y);
		//ExcelRpt_SetCellValue (worksheetHandleCoverageMatrix, range, CAVT_CSTRING,"oui");
		ExcelRpt_AddHyperlink (worksheetHandleCoverageMatrix, range, currentDirPath, currentDirPath);
		ExcelRpt_RangeBorder (worksheetHandleCoverageMatrix, range, 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);

		//colonne Check_Results.txt
		sprintf(range,"D%d",y);
		if (currentTestStatus == -1)
		{
			strcat(currentDirPath,"\\ErrorLog.txt");
		}
		else
		{
			strcat(currentDirPath,"\\Check_Results.txt");
		}
		ExcelRpt_SetCellValue (worksheetHandleCoverageMatrix, range, CAVT_CSTRING,"click here");
		ExcelRpt_AddHyperlink (worksheetHandleCoverageMatrix, range, currentDirPath,"click here");
		ExcelRpt_RangeBorder (worksheetHandleCoverageMatrix, range, 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);


		//colonne Status
		sprintf(range,"B%d",y);
		//PASSED
		if (currentTestStatus == 1)
		{
			ExcelRpt_SetCellValue (worksheetHandleCoverageMatrix, range, CAVT_CSTRING,"PASSED");
			ExcelRpt_SetCellRangeAttribute (worksheetHandleCoverageMatrix, range, ER_CR_ATTR_BGCOLOR, 0x6AA84FL);  //green color
		}
		// FAILED
		if (currentTestStatus == 0)
		{
			ExcelRpt_SetCellValue (worksheetHandleCoverageMatrix, range, CAVT_CSTRING,"FAILED");
			ExcelRpt_SetCellRangeAttribute (worksheetHandleCoverageMatrix, range, ER_CR_ATTR_BGCOLOR, 0xCC0000L);  //red color
		}
		// ERROR
		if (currentTestStatus == -1)
		{
			ExcelRpt_SetCellValue (worksheetHandleCoverageMatrix, range, CAVT_CSTRING,"ERROR");
			ExcelRpt_SetCellRangeAttribute (worksheetHandleCoverageMatrix, range, ER_CR_ATTR_BGCOLOR, 0xF1C232L); //yellow color

		}
		ExcelRpt_RangeBorder (worksheetHandleCoverageMatrix, range, 1, 0x000000L , ExRConst_Medium, ExRConst_EdgeTop | ExRConst_EdgeBottom | ExRConst_EdgeLeft | ExRConst_EdgeRight);

		
	}


}

//*****************************************************************************************************
// MODIF MaximePAGES 27/06/2020 - Function : numeric keypad  dimmed for parameters for script creation

void dimmedNumericKeypadForScript(int state)
{

	//show the "insert parameters
	SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_DECORATION_6,ATTR_DIMMED, state);
	SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_TEXTMSG,ATTR_DIMMED, state);
	SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_PARAMETERS,ATTR_DIMMED, state);


	SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_FORMULA,ATTR_DIMMED, state);
	SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_DURATION,ATTR_DIMMED, state);


	SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_VALUE,ATTR_DIMMED, state);
	SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_UNIT,ATTR_DIMMED, state);
	SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_TIME,ATTR_DIMMED, state);
	SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_TEXTMSG_2,ATTR_DIMMED, state);



	//operator
	SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_BOUTONPAR1,ATTR_DIMMED, state);
	SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_BOUTONPAR2,ATTR_DIMMED, state);
	SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_BOUTONADD,ATTR_DIMMED, state);
	SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_BOUTONSOU,ATTR_DIMMED, state);
	SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_BOUTONMUL,ATTR_DIMMED, state);
	SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_BOUTONDIV,ATTR_DIMMED, state);

	//dot
	SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_BOUTONDOT,ATTR_DIMMED, state);

	//numbers
	SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_BOUTON0,ATTR_DIMMED, state);
	SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_BOUTON1,ATTR_DIMMED, state);
	SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_BOUTON2,ATTR_DIMMED, state);
	SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_BOUTON3,ATTR_DIMMED, state);
	SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_BOUTON4,ATTR_DIMMED, state);
	SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_BOUTON5,ATTR_DIMMED, state);
	SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_BOUTON6,ATTR_DIMMED, state);
	SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_BOUTON7,ATTR_DIMMED, state);
	SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_BOUTON8,ATTR_DIMMED, state);
	SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_BOUTON9,ATTR_DIMMED, state);

	//insert
	SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_INSERT1BIS,ATTR_DIMMED, state);
	
	//dell
	SetCtrlAttribute(GiPopupAdd2,PANEL_ADD_BOUTONDEL,ATTR_DIMMED, state);  
}


//*****************************************************************************************************
// MODIF MaximePAGES 27/06/2020 - Function : numeric keypad  dimmed for parameters for Expectud Result creation

void dimmedNumericKeypadForExp (int state)
{

	//show the "insert parameters
	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DECORATION_5,ATTR_DIMMED, state);
	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_8,ATTR_DIMMED, state);
	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_SELECTPARAM,ATTR_DIMMED, state);


	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_FORMUEXPEC,ATTR_DIMMED, state);
	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_EXPVALUE_2,ATTR_DIMMED, state);


	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_EXPVALUE,ATTR_DIMMED, state);
	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_EXPUNIT,ATTR_DIMMED, state);
	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_TEXTMSG_7,ATTR_DIMMED, state);



	//operator
	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTPAR1,ATTR_DIMMED, state);
	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTPAR2,ATTR_DIMMED, state);
	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTSUM,ATTR_DIMMED, state);
	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTSUB,ATTR_DIMMED, state);
	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTMULT,ATTR_DIMMED, state);
	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTDIV,ATTR_DIMMED, state);

	//dot
	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTDOT,ATTR_DIMMED, state);

	//numbers
	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUT0,ATTR_DIMMED, state);
	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUT1,ATTR_DIMMED, state);
	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUT2,ATTR_DIMMED, state);
	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUT3,ATTR_DIMMED, state);
	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUT4,ATTR_DIMMED, state);
	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUT5,ATTR_DIMMED, state);
	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUT6,ATTR_DIMMED, state);
	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUT7,ATTR_DIMMED, state);
	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUT8,ATTR_DIMMED, state);
	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUT9,ATTR_DIMMED, state);

	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTA,ATTR_DIMMED, state);
	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTB,ATTR_DIMMED, state); 
	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTC,ATTR_DIMMED, state); 
	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTD,ATTR_DIMMED, state); 
	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTE,ATTR_DIMMED, state); 
	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_BUTF,ATTR_DIMMED, state); 
	
	//dell
	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_DELETEBUTTON,ATTR_DIMMED, state);

	//insert
	SetCtrlAttribute(GiExpectedResultsPanel,EXPRESULTS_INSERTFORMULE,ATTR_DIMMED, state);
}


//*****************************************************************************************************
// MODIF MaximePAGES 03/08/2020 - Functions to check is a string is a number or not.
int isNumber(char * mynum)
{

	int foundChar = 0;
	int n = 0;
	int isnumber = 1;

	while ( (foundChar == 0) && (mynum[n]!= '\0') )
	{
		if ( isdigit(mynum[n]) == 0 )
		{
			foundChar = 1;
			isnumber = 0;
		}
		n++;
	}
	return isnumber;
}


/*
//*****************************************************************************************************
// MODIF MaximePAGES 28/06/2020 - Functions to translate a string with parameter(s) to a single value
float parameterToValue (char * formula)
{

	float valueReturned = 0;
	int i=0;
	int valueDebug = 0;
	char Formula[500]="";
	const char s[2] = " ";
	char *token;
	char *tabtoken[200] = {NULL};
	char range[30];
	char parameter[100]="";
	char unitParameter[50]="";
	char Value[100];
	char *TabValue[100] = {NULL};
	int boucle=0;
	int end;
	int StringEqual=0;
	BOOL bParam = FALSE;
	char defaultDirectory[MAX_PATHNAME_LEN] = "";

	strncpy (ChaineExcel, "=", 200);

	strcpy(Formula, formula); //Ex Formula1 = "MD_Interburst + 4"

	/// get the first token

	if ( strcmp(Formula,"") == 0)
	{
		strcpy(Formula,"0");
	}



	if (isNumber(Formula) == 0)
	{


		token = strtok(Formula, s);

		for(int row=0; row < nbParameter; row++)
		{
			StringEqual=strcmp(myParameterTab[row].name, token);

			if(StringEqual == 0)
			{

				strcat(ChaineExcel, myParameterTab[row].value);
				bParam = TRUE;

				//save unit parameter
				strcpy(unitParameter,myParameterTab[row].unit);
				StringEqual = 1;
				break;
			}

		}


		if (bParam==FALSE)
		{
			strcat(ChaineExcel,token);
		}





		//walk through other tokens
		while( token != NULL )
		{
			token = strtok(NULL, s);
			bParam = FALSE;

			if(token!=NULL)
			{
				for(int row=0; row < nbParameter; row++)
				{

					StringEqual=strcmp(myParameterTab[row].name, token);


					if(StringEqual == 0)
					{
						//TabValue[i] = myParameterTab[row].value;
						strcat(ChaineExcel, myParameterTab[row].value);
						bParam = TRUE;

						//save unit parameter
						strcpy(unitParameter,myParameterTab[row].unit);
						break;
					}
				}

				if (bParam==FALSE)
				{
					strcat(ChaineExcel, token);
				}
			}
		}



		// Ouverture d'excel
		GetProjectDir(defaultDirectory);
		strcat(defaultDirectory,"\\CalculationFile.xlsx");
		ExcelRpt_WorkbookOpen (applicationHandleProject,defaultDirectory, &workbookHandle5);
		ExcelRpt_GetWorksheetFromIndex(workbookHandle5, 1, &worksheetHandle5);

		// calcule la valeur sur excel puis retourne cette valeur
		ExcelRpt_SetCellValue (worksheetHandle5, "A1", CAVT_CSTRING, ChaineExcel);
		ExcelRpt_GetCellValue (worksheetHandle5, "A1", CAVT_FLOAT, &valueReturned);

		// close workbookHandle5
		ExcelRpt_WorkbookClose (workbookHandle5, 0);

		//unit condition
		if((strcmp(unitParameter, "[ms]")) == 0)
		{
			valueReturned = valueReturned/1000;
		}

		if((strcmp(unitParameter, "[State]")) == 0)
		{
			valueReturned = (int)valueReturned;
		}


	}
	else
	{
		valueReturned = atof(Formula);
	}

	return valueReturned;
}
*/


//*****************************************************************************************************
// MODIF MaximePAGES 28/06/2020 - Functions to translate a string with parameter(s) to a single value (string type)
char * parameterToValue_Str (char * formula)
{

	char valueReturned[200]= "";


	char Formula[500]="";
	const char s[2] = " ";
	char *token;

	

	char unitParameter[50]="";
	char valueParameter[25]="";
	
	
	

	int StringEqual=0;
	int charInsideParam=0;
	BOOL bParam = FALSE;
	char defaultDirectory[MAX_PATHNAME_LEN] = "";
	float tempValue = 0.0;

	strncpy (ChaineExcel, "=", 200);

	strcpy(Formula, formula); //Ex Formula1 = "MD_Interburst + 4"


	if ( strcmp(Formula,"") == 0)
	{
		strcpy(Formula,"0");
	}



	if (isNumber(Formula) == 0)
	{


		//********************
		char * rest = Formula;
		while ((token = strtok_r(rest, s, &rest)))
		{
			bParam = FALSE;
			for(int row=0; row < nbParameter; row++)
			{

			//	printf("name = %s\n", myParameterTab[row].name);
				StringEqual=strcmp(myParameterTab[row].name, token);


				if(StringEqual == 0)
				{

					bParam = TRUE;

					//save value parameter 
					strcpy(valueParameter,myParameterTab[row].value);
					
					//save unit parameter
					strcpy(unitParameter,myParameterTab[row].unit);

					if ( (strcmp(unitParameter,"[State]") == 0) || (strcmp(unitParameter,"[SW Ident Frame Parameters]") == 0))
					{
						charInsideParam = 1;
						strcpy(valueReturned,myParameterTab[row].value);
					}
					else
					{

						if  (strcmp(unitParameter,"[ms]") == 0)  //ms to sec conversion
						{
							tempValue = atof(myParameterTab[row].value);
							tempValue = tempValue/1000;
							sprintf(valueParameter,"%.5f",tempValue);
						} 


						strcat(ChaineExcel, valueParameter);
					}


					break;
				}
			}

			if (bParam==FALSE)
			{
				strcat(ChaineExcel, token);
				strcpy(valueReturned,ChaineExcel);
			}

		}


		if (charInsideParam == 0)
		{


			// Ouverture d'excel
			GetProjectDir(defaultDirectory);
			strcat(defaultDirectory,"\\CalculationFile.xlsx");
			ExcelRpt_WorkbookOpen (applicationHandleProject,defaultDirectory, &workbookHandle5);
			ExcelRpt_GetWorksheetFromIndex(workbookHandle5, 1, &worksheetHandle5);

			// calcule la valeur sur excel puis retourne cette valeur
			ExcelRpt_SetCellValue (worksheetHandle5, "A1", CAVT_CSTRING, ChaineExcel);
			ExcelRpt_GetCellValue (worksheetHandle5, "A1", CAVT_CSTRING, valueReturned);

			// close workbookHandle5
			ExcelRpt_WorkbookClose (workbookHandle5, 0);
		}


	}
	else
	{

		strcpy(valueReturned,Formula);
	}

	return valueReturned;
}


//*****************************************************************************************************
// MODIF MaximePAGES 29/07/2020 - Function to update the END time inside the script excel definition.

void translateScriptExcel (char * excelScript, char * newExcelScript)
{

	int iTime = 0;
	int iDuration = 0;
	
	
	char sTime[100]= ""; 
	char sDuration[100]= ""; 
	
	char wuIDname[100] = ""; 
	char wuIDvalue[100] = "";
	char WUIDnameFromDatabase[100]; 
	int end=1;




	char sRange1[100] = "";
	char sRange2[100] = "";
	
	char sValue[100];

	


	char sTol[100]= ""; 


	char sTolp[100]= ""; 

	char sElement[100];
	char *token;
	

	char range[30];
	char rangewuid[30];
	int type = 0;

	int endtimeplus = 0;
	int sum = 0;
	int finalsum = 0;
	int nbOfRow_TestCase = 0;
	int nbOfRow_ExpResult= 0;
	char chaineValue[100] = "";


	ExcelRpt_WorkbookOpen (applicationHandleProject,excelScript, &workbookHandle10);

	// worksheet n°1 - TestCase
	ExcelRpt_GetWorksheetFromIndex(workbookHandle10, 1, &worksheetHandle10_1);
	ExcelRpt_GetCellValue (worksheetHandle10_1, "I1", CAVT_INT, &nbOfRow_TestCase);
	ExcelRpt_GetCellValue (worksheetHandle10_1, "I2", CAVT_INT, &endtimeplus);


	for (int row = 2 ; row <= nbOfRow_TestCase ; row++)
	{

		//TIME COLUMN
		sprintf(range,"A%d",row);
		ExcelRpt_GetCellValue (worksheetHandle10_1, range, CAVT_CSTRING, sTime);
		//fTime = parameterToValue(sTime);
		sprintf(sTime,"%s",parameterToValue_Str(sTime));
		ExcelRpt_SetCellValue (worksheetHandle10_1, range, CAVT_CSTRING, sTime);

		//DURATION COLUMN
		sprintf(range,"B%d",row);
		ExcelRpt_GetCellValue (worksheetHandle10_1, range, CAVT_CSTRING, sDuration);
		//fDuration = parameterToValue(sDuration);
		sprintf(sDuration,"%s",parameterToValue_Str(sDuration));
		ExcelRpt_SetCellValue (worksheetHandle10_1, range, CAVT_CSTRING, sDuration);

		
		//WU ID COLUMN ** Modif MaximePAGES 13/08/2020 -  WU ID translation
		sprintf(range,"F%d",row);
		ExcelRpt_GetCellValue (worksheetHandle10_1, range, CAVT_CSTRING, wuIDname);

		if (strcmp(wuIDname, "") != 0)
		{
			for(int i=2; end==1; i++)
			{
				sprintf(rangewuid,"O%d",i);		//laaa coder en dur !!!
				ExcelRpt_GetCellValue (worksheetHandledata1, rangewuid, CAVT_CSTRING, WUIDnameFromDatabase);

				if (strcmp(WUIDnameFromDatabase, "") == 0)
				{
					end=0;  //the end was found
				}
				else
				{
					if (strcmp(wuIDname, WUIDnameFromDatabase) == 0)
					{
						sprintf(rangewuid,"P%d",i);
						ExcelRpt_GetCellValue (worksheetHandledata1, rangewuid, CAVT_CSTRING, wuIDvalue);
						end =0;
					}
					else 
					{
						strcpy(wuIDvalue,"");	
					}
				}
			}
		}
		else 
		{
			strcpy(wuIDvalue,"");	
		}	

		ExcelRpt_SetCellValue (worksheetHandle10_1, range, CAVT_CSTRING, wuIDvalue);

		end = 1;


		iTime = atoi(sTime);
		iDuration = atoi(sDuration);

		//calculation of the largest sum
		sum = iTime + iDuration;
		if(finalsum <= sum)
		{
			finalsum = sum;
			iTime = 0;
			iDuration = 0;
		}

	}

	finalsum += endtimeplus;
	sprintf(range,"A%d",nbOfRow_TestCase+1);
	ExcelRpt_SetCellValue (worksheetHandle10_1, range, CAVT_INT, finalsum);


	// worksheet n°2 - ExpectedResults
	ExcelRpt_GetWorksheetFromIndex(workbookHandle10, 2, &worksheetHandle10_2);
	ExcelRpt_GetCellValue (worksheetHandle10_2, "I1", CAVT_INT, &nbOfRow_ExpResult);

	for (int row2 = 2 ; row2 <= nbOfRow_ExpResult ; row2++)
	{

		//VALUE COLUMN
		sprintf(range,"F%d",row2);
		ExcelRpt_GetCellValue (worksheetHandle10_2, range, CAVT_CSTRING, sValue);

		strcpy(chaineValue,"");
		type = typeOfValue(sValue); //if 1 then signle or seq, if 2 then range

		if (type == 1) //signle value or seq
		{
			char * rest = sValue;
			while ((token = strtok_r(rest, ";", &rest)))
			{
				sprintf(sElement,"%s",parameterToValue_Str(token));
				strcat(chaineValue,sElement);
				strcat(chaineValue,";");
			}
		}

		if (type == 2) // range values
		{
			RetrieveandSeparateRange(sValue);
			strtok(range1 , ";");
			strtok(range2 , ";");
			sprintf(sRange1,"%s",parameterToValue_Str(range1));
			sprintf(sRange2,"%s",parameterToValue_Str(range2));
			
			sprintf(chaineValue,"%s:%s;",sRange1,sRange2);
		}


		ExcelRpt_SetCellValue (worksheetHandle10_2, range, CAVT_CSTRING, chaineValue);

		//TOLERENCE COLUMN
		sprintf(range,"G%d",row2);
		ExcelRpt_GetCellValue (worksheetHandle10_2, range, CAVT_CSTRING, sTol);
		strtok(sTol,";");
		//fTol = atof(parameterToValue_Str(sTol));
		sprintf(sTol,"%s;",parameterToValue_Str(sTol));
		ExcelRpt_SetCellValue (worksheetHandle10_2, range, CAVT_CSTRING, sTol);

		//TOLERENCE% COLUMN
		sprintf(range,"H%d",row2);
		ExcelRpt_GetCellValue (worksheetHandle10_2, range, CAVT_CSTRING, sTolp);
		strtok(sTolp,";");
		//fTolp = atof(parameterToValue_Str(sTolp));
		sprintf(sTolp,"%s;",parameterToValue_Str(sTolp));
		ExcelRpt_SetCellValue (worksheetHandle10_2, range, CAVT_CSTRING, sTolp);

	}


//saving and closing  workbook
	ExcelRpt_WorkbookSave (workbookHandle10, newExcelScript, ExRConst_DefaultFileFormat);
	ExcelRpt_WorkbookClose (workbookHandle10, 0);


}





//*****************************************************************************************************
// MODIF MaximePAGES 31/07/2020 - Function to generate a tab of parameters from Database
// MODIF MarvynPANNETIER 13/072021
void generateParameterTab (CAObjHandle worksheetHandle)

{
	int end = 1;
	char range1[30];
	char range2[30];
	char range3[30];
	
	//printf("nbparam = %d\n",nbParameter);

	for(int row=2; row <= nbParameter+1; row++)
	{
		sprintf(range1,"A%d",row);
		ExcelRpt_GetCellValue (worksheetHandle, range1, CAVT_CSTRING, myParameterTab[row-2].name);

		sprintf(range2,"B%d",row);
		ExcelRpt_GetCellValue (worksheetHandle, range2, CAVT_CSTRING, myParameterTab[row-2].value);

		sprintf(range3,"C%d",row);
		ExcelRpt_GetCellValue (worksheetHandle, range3, CAVT_CSTRING, myParameterTab[row-2].unit);

		//printf("name = %s\nvalue = %s\nunit = %s\n ROW = %d\n\n",myParameterTab[row-2].name,myParameterTab[row-2].value,myParameterTab[row-2].unit,row-2);
		
		if(strcmp(myParameterTab[row-2].name, "") == 0)
		{
			end=0;  //the end was found
		}
		
	}
}

//*****************************************************************************************************
// MODIF MaximePAGES 23/06/2020 - Functions to generate a log file (for debug only)

void writeLogDebugFile(char * commentaire, float other, char * filesource, int line)
{
	char myfilepath[MAX_PATHNAME_LEN];
	FILE * fichier = NULL;
	time_t now;
	int h, min, s;


	GetProjectDir(myfilepath);
	strcat(myfilepath,"\\LogDebugFile.txt"); //we define  the name of the file
	fichier = fopen(myfilepath,"a");

	if (fichier != NULL)
	{
		time(&now);
		struct tm *local = localtime(&now);
		h = local->tm_hour;
		min = local->tm_min;
		s = local->tm_sec;

		fprintf(fichier,"%02d:%02d:%02d		comment:\"%-30s\"	other: %-30f		file: %-30s	line: %-10d \n", h, min, s,commentaire,other,filesource,line );

		fclose(fichier);
	}

}

//*****************************************************************************************************
// MODIF MaximePAGES 05/08/2020 - similar to strotok but accepts multi thread
char * strtok_r (char *s, const char *delim, char **save_ptr)
{
	char *end;
	if (s == NULL)
		s = *save_ptr;
	if (*s == '\0')
	{
		*save_ptr = s;
		return NULL;
	}
	/* Scan leading delimiters.  */
	s += strspn (s, delim);
	if (*s == '\0')
	{
		*save_ptr = s;
		return NULL;
	}
	/* Find the end of the token.  */
	end = s + strcspn (s, delim);
	if (*end == '\0')
	{
		*save_ptr = end;
		return s;
	}
	/* Terminate the token and make *SAVE_PTR point past it.  */
	*end = '\0';
	*save_ptr = end + 1;
	return s;
}


//*****************************************************************************************************
// MODIF MaximePAGES 11/08/2020 - comparing two text files to know if the current seq is the last saved one. 

int compareFile(FILE * fPtr1, FILE * fPtr2, int * line, int * col)
{
    char ch1, ch2;

    *line = 1;
    *col  = 0;

    do
    {
        // Input character from both files
        ch1 = fgetc(fPtr1);
        ch2 = fgetc(fPtr2);
        
        // Increment line 
        if (ch1 == '\n')
        {
            *line += 1;
            *col = 0;
        }

        // If characters are not same then return -1
        if (ch1 != ch2)
            return -1;

        *col  += 1;

    } while (ch1 != EOF && ch2 != EOF);


    /* If both files have reached end */
    if (ch1 == EOF && ch2 == EOF)
        return 0;
    else
        return -1;
}


//*****************************************************************************************************
// MODIF MaximePAGES 11/08/2020 - check if the current seq is equal to the last saved one. 

int checkCurrentSeqSaved (char *sProjectDir)
{


	char *confpath; 
	int diff;
	
	confpath = strcat(sProjectDir,"\\Config\\tempSeq.txt");
	
	
	if(!CheckPrecondition())
		return -1;


	int f = OpenFile (confpath, VAL_WRITE_ONLY, VAL_TRUNCATE, VAL_ASCII);

	int a =  WriteLine (f,GsAnumCfgFile,-1);
	int b =  WriteLine (f,GsCheckCfgFile,-1);
	int c =  WriteLine (f,GsLogPathInit,-1);

	if(a != -1 && b != -1 && c != -1)
	{
		//MessagePopup("Message","Paths Configuration File Save Succeded!");
	}
	else
	{
		Beep();
		MessagePopup("Error","Cannot compare the current seq and the last saved one!");
		CloseFile(f);
		return -1;
	}
	int rowNo;
	GetNumTableRows(GiPanel, PANEL_MODE_TABLE_SCRIPT,&rowNo);

	if(WriteLine (f,"",-1) == -1)
	{
		MessagePopup("Error","Error Encountered while comparing the current seq and the last saved one!");
		CloseFile(f);
		return -1;
	}
	if(WriteLine (f,"Tests:",-1) == -1)
	{
		MessagePopup("Error","Error Encountered while comparing the current seq and the last saved one!");
		CloseFile(f);
		return -1;
	}

	int i;
	Point cell;
	for(i = 1; i<= rowNo; i++)
	{
		cell.x = 5;
		cell.y = i;
		char auxString[MAX_PATHNAME_LEN];
		char auxStrNum[MAX_PATHNAME_LEN];

		GetTableCellVal (GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, auxString);
		cell.x = 4;
		GetTableCellVal (GiPanel, PANEL_MODE_TABLE_SCRIPT, cell, auxStrNum);
		//int auxNum = 0;

		if(WriteLine (f,auxString,-1) == -1)
		{

			MessagePopup("Error","Error Encountered while comparing the current seq and the last saved one!");
			CloseFile(f);
			return -1;
		}

		if(WriteLine (f,auxStrNum,-1) == -1)
		{
			MessagePopup("Error","Error Encountered while comparing the current seq and the last saved one!");
			CloseFile(f);
			return -1;
		}
	}
	CloseFile(f);

	// comparing the current seq and the last saved one
	FILE * fPtr1; 
    FILE * fPtr2;

	/*  Open all files to compare */
	fPtr1 = fopen(GsSaveConfigPathsFile, "r");  //last seq saved
    fPtr2 = fopen(confpath, "r");  // current seq 

	/* fopen() return NULL if unable to open file in given mode. */
	if (fPtr1 == NULL || fPtr2 == NULL)
	{
		/* Unable to open file hence exit */
		MessagePopup("Error","Error Encountered while comparing sequence files!");
		fclose(fPtr1);
		fclose(fPtr2);
		return -1;
	}

	/* Call function to compare file */
	int line;
	int col;
    diff = compareFile(fPtr1, fPtr2, &line, &col);
	
	/* Finally close files to release resources */
    fclose(fPtr1);
    fclose(fPtr2);

    return diff;
	
}


int CheckIndivSTDEV()  //char *pathname2
{
	int failResult = 1;
	int failResult0 = 1;  
	int failResult1 = 1;  
	int failResult2 = 1;  
	int failResult3 = 1;  
	int failResult4 = 1;
	
	int iAnglePosition = 0;
	double dAngleValue = 0.0;
	
	double sum0 = 0.0;
	double sum1 = 0.0; 
	double sum2 = 0.0; 
	double sum3 = 0.0; 
	double sum4 = 0.0;
	int count0 = 0;
	int count1 = 0; 
	int count2 = 0; 
	int count3 = 0; 
	int count4 = 0; 
	double distance0 = 0.0;
	double distance1 = 0.0;  
	double distance2 = 0.0;  
	double distance3 = 0.0;
	double distance4 = 0.0; 
	
	double average0 =0.0;
	double average1 =0.0;  
	double average2 =0.0;  
	double average3 =0.0; 
	double average4 =0.0; 
	
	double result0 = 0.0;
	double result1 = 0.0; 
	double result2 = 0.0; 
	double result3 = 0.0; 
	double result4 = 0.0; 
	
	double findmin0 = 12345.0;
	double findmin1 = 12345.0; 
	double findmin2 = 12345.0; 
	double findmin3 = 12345.0; 
	double findmin4 = 12345.0; 
	
	double findmax0 = 0.0;
	double findmax1 = 0.0; 
	double findmax2 = 0.0; 
	double findmax3 = 0.0; 
	double findmax4 = 0.0; 
	
	
	
	int zeroFound = 0;
	int oneFound = 0;
	int twoFound = 0;
	int threeFound = 0;
	int fourFound = 0;
	
	int nbFrame = atoi(myFrameTab[0].angleval);
	double dRange1 = 0.0, dRange2 =0.0; 

	dRange1 = atof(range1); //range1 and range2 are global variables
	dRange2 = atof(range2);
	
	fprintf(file_handle, "INDIVIDUAL STDEV ***\n\n");
	
	//Average calculation ****************
	for (int i = 1 ; i <= nbFrame ; i++)
	{
		
		//printf("myFrameTab[%d].anglePos = %s and myFrameTab[%d].angleval = %s\n",i,myFrameTab[i].anglepos,i,myFrameTab[i].angleval);
		iAnglePosition = atoi(myFrameTab[i].anglepos);
		dAngleValue = atof(myFrameTab[i].angleval)  ;
		
		switch(iAnglePosition)
		{
			case 0:
				count0++;
				sum0 += dAngleValue;
				break;

			case 1:
				count1++;
				sum1 += dAngleValue;
				break;

			case 2:
				count2++;
				sum2 += dAngleValue;
				break;

			case 3:
				count3++;
				sum3 += dAngleValue;
				break;
			case 4:
				count4++;
				sum4 += dAngleValue;
				break;
				
		}  //switch
		
		
	}
	
	if (count0 > 0) 
	{
		zeroFound = 1;
		average0 = sum0/count0;	
	}
	
	if (count1 > 0) 
	{
		oneFound = 1;
		average1 = sum1/count1;	
	}
	
	if (count2 > 0) 
	{
		twoFound = 1;
		average2 = sum2/count2;	
	}
	
	if (count3 > 0) 
	{
		threeFound = 1;
		average3 = sum3/count3;	
	}
	if (count4 > 0) 
	{
		fourFound = 1; 
		average4 = sum4/count4;	
	}
	
	
	//raz
	count0 = 0;
	count1 = 0;
	count2 = 0;
	count3 = 0;
	count4 = 0;
	sum0 = 0.0;
	sum1 = 0.0; 
	sum2 = 0.0; 
	sum3 = 0.0; 
	sum4 = 0.0; 

	
	//STDEV calculation **************** 
	for (int j = 1 ; j <= nbFrame ; j++)
	{
		
		iAnglePosition = atoi(myFrameTab[j].anglepos);
		dAngleValue = atof(myFrameTab[j].angleval);
		
		switch(iAnglePosition)
		{
			case 0:

				//find min and max values between labels  
				if(findmin0 >= dAngleValue )
				{
					findmin0 = dAngleValue;   
				}
				if(findmax0 <= dAngleValue )
				{
					findmax0 = dAngleValue;   
				}

				count0++;
				distance0 = ((dAngleValue - average0)*(dAngleValue - average0));
				sum0 += distance0;
				break;

			case 1:
				
				//find min and max values between labels  
				if(findmin1 >= dAngleValue )
				{
					findmin1 = dAngleValue;   
				}
				if(findmax1 <= dAngleValue )
				{
					findmax1 = dAngleValue;   
				}
				
				count1++;
				distance1 = ((dAngleValue - average1)*(dAngleValue - average1));
				sum1 += distance1;
				break;

			case 2:
				
				//find min and max values between labels  
				if(findmin2 >= dAngleValue )
				{
					findmin2 = dAngleValue;   
				}
				if(findmax2 <= dAngleValue )
				{
					findmax2 = dAngleValue;   
				}
				
				count2++;
				distance2 = ((dAngleValue - average2)*(dAngleValue - average2));
				sum2 += distance2;
				break;

			case 3:
				
				//find min and max values between labels  
				if(findmin3 >= dAngleValue )
				{
					findmin3 = dAngleValue;   
				}
				if(findmax3 <= dAngleValue )
				{
					findmax3 = dAngleValue;   
				}
				
				count3++;
				distance3 = ((dAngleValue - average3)*(dAngleValue - average3));
				sum3 += distance3;
				break;
				
			case 4:
				
				//find min and max values between labels  
				if(findmin4 >= dAngleValue )
				{
					findmin4 = dAngleValue;   
				}
				if(findmax4 <= dAngleValue )
				{
					findmax4 = dAngleValue;   
				}
				
				count4++;
				distance4 = ((dAngleValue - average4)*(dAngleValue - average4));
				sum4 += distance4;
				break;
				
		}  //switch
	}
	
	
	if((count0 > 0) && (zeroFound == 1))
	{
		result0 = 3*sqrt(sum0/count0);
		fprintf(file_handle, "Angle_Position 0:\n"); 
		fprintf(file_handle, "Nb frames: %d\n", count0); 
		fprintf(file_handle, "STDEV * 3 : %f ; Min = %f ; Max = %f\n", result0,findmin0,findmax0);
		if(result0 < dRange1 || result0 > dRange2)  //range
		{
			//failed
			failResult0 = 0;
			fprintf(file_handle, "STDEV not between ranges ... FAILED!\n\n");
		}
		else
		{
			//succes 3*sigma between the ranges
			failResult0 = 1; 
			fprintf(file_handle, "STDEV  between ranges ... PASSED!\n\n");
		}

	}
	
	if((count1 > 0) && (oneFound == 1))
	{
		result1 = 3*sqrt(sum1/count1);
		fprintf(file_handle, "Angle_Position 1:\n"); 
		fprintf(file_handle, "Nb frames: %d\n", count1); 
		fprintf(file_handle, "STDEV * 3 : %f ; Min = %f ; Max = %f\n", result1,findmin1,findmax1);
		if(result1 < dRange1 || result1 > dRange2)  //range
		{
			//failed
			failResult1 = 0;
			fprintf(file_handle, "STDEV not between ranges ... FAILED!\n\n");
		}
		else
		{
			//succes 3*sigma between the ranges
			failResult1 = 1;
			fprintf(file_handle, "STDEV  between ranges ... PASSED!\n\n");
		}

	}
	
	if((count2 > 0) && (twoFound == 1))
	{
		result2 = 3*sqrt(sum2/count2);
		fprintf(file_handle, "Angle_Position 2:\n"); 
		fprintf(file_handle, "Nb frames: %d\n", count2); 
		fprintf(file_handle, "STDEV * 3 : %f ; Min = %f ; Max = %f\n", result2,findmin2,findmax2);

		if(result2 < dRange1 || result2 > dRange2)  //range
		{
			//failed
			failResult2 = 0;
			fprintf(file_handle, "STDEV not between ranges ... FAILED!\n\n");
		}
		else
		{
			//succes 3*sigma between the ranges
			failResult2 = 1;
			fprintf(file_handle, "STDEV  between ranges ... PASSED!\n\n");
		}

	}
	
	if((count3 > 0) && (threeFound == 1))
	{
		result3 = 3*sqrt(sum3/count3);
		fprintf(file_handle, "Angle_Position 3:\n"); 
		fprintf(file_handle, "Nb frames: %d\n", count3); 
		fprintf(file_handle, "STDEV * 3 : %f ; Min = %f ; Max = %f\n", result3,findmin3,findmax3);

		if(result3 < dRange1 || result3 > dRange2)  //range
		{
			//failed
			failResult3 = 0;
			fprintf(file_handle, "STDEV not between ranges ... FAILED!\n\n");
		}
		else
		{
			//succes 3*sigma between the ranges
			failResult3 = 1;
			fprintf(file_handle, "STDEV  between ranges ... PASSED!\n\n");
		}

	}
	if((count4 > 0) && (fourFound == 1))
	{
		result4 = 3*sqrt(sum4/count4);
		fprintf(file_handle, "Angle_Position 4:\n"); 
		fprintf(file_handle, "Nb frames: %d\n", count4); 
		fprintf(file_handle, "STDEV * 3 : %f ; Min = %f ; Max = %f\n", result4,findmin4,findmax4);

		if(result4 < dRange1 || result4 > dRange2)  //range
		{
			//failed
			failResult4 = 0;
			fprintf(file_handle, "STDEV not between ranges ... FAILED!\n\n");
		}
		else
		{
			//succes 3*sigma between the ranges
			failResult4 = 1;
			fprintf(file_handle, "STDEV  between ranges ... PASSED!\n\n");
		}

	}
	
	
	
	failResult = !( failResult0*failResult1*failResult2*failResult3*failResult4 );
	
		
	//0 if passed 
	//1 if failed
	return failResult;

}

int CheckNewSTDEV(char *sAngleEmission,int errorPreSTDEV,int errorIndivSTDEV)  //char *pathname2
{
	int failResult = 1;
	
	int auxFailRes = 0; 
	
	int iAnglePosition = 0;
	double dAngleValue = 0.0;
	
	double sum = 0.0;
	int count = 0;
	double distance = 0.0;
	double average =0.0;
	double result = 0.0;
	
	//double findmin = 5000.0;
	//double findmax = 0.0;

	
	int iAngleEmission = 0;

	int nbFrame = atoi(myFrameTab[0].angleval);
	double dRange1 = 0.0, dRange2 =0.0; 

	dRange1 = atof(range1); //range1 and range2 are global variables
	dRange2 = atof(range2);
	
	iAngleEmission = atoi(sAngleEmission);
	
	fprintf(file_handle, "GLOBAL STDEV ***\n\n");
	
	//Average calculation ****************
	for (int i = 1 ; i <= nbFrame ; i++)
	{
		dAngleValue = atof(myFrameTab[i].angleval);
		iAnglePosition = atoi(myFrameTab[i].anglepos);
		
		switch(iAngleEmission)
		{
			case 1:
				count++;
				sum += dAngleValue;
				break;

			case 2:
				count++;
				dAngleValue = dAngleValue - (iAnglePosition*90);
				if (dAngleValue<0)
				{
					dAngleValue = 360 + dAngleValue;
				}
				else
				{
					dAngleValue	= (int)dAngleValue%360;
				}

				sum += dAngleValue;
				break;

			case 4:
				count++;
				dAngleValue = dAngleValue - (iAnglePosition*90);
				if (dAngleValue<0)
				{
					dAngleValue = 360 + dAngleValue;
				}
				else
				{
					dAngleValue	= (int)dAngleValue%360;
				}

				sum += dAngleValue;
				break;
		}  //switch
	}
	
	
	fprintf(file_handle, "Nb synchronized frames: %d\n",count);   
	if (count > 0) 
	{
		average = sum/count;	
	}

	if(count < 9)  //Modif MaximePAGES 09/09/2020 - we need minimum 9 frames to calculate the LSE
	{
		//failed
		auxFailRes = 1;
		fprintf(file_handle, "Not enought frames. Need at least 9 synchronized frames ... FAILED!\n");
	}


	//raz
	count = 0;
	sum = 0.0;

	
	//STDEV calculation **************** 
	for (int j = 1 ; j <= nbFrame ; j++)
	{
		dAngleValue = atof(myFrameTab[j].angleval)  ;
		iAnglePosition = atoi(myFrameTab[j].anglepos); 

		/*
		if(findmin >= dAngleValue )
		{
			findmin = dAngleValue;   //find min value between labels
		}
		if(findmax <= dAngleValue )
		{
			findmax = dAngleValue;   //find max value between labels
		}  
		*/

		switch(iAngleEmission)
		{
			case 1:
				count++;
				distance = ((dAngleValue - average)*(dAngleValue - average));
				sum += distance;
				break;

			case 2:
				count++;
				dAngleValue = dAngleValue - (iAnglePosition*90);
				if (dAngleValue<0)
				{
					dAngleValue = 360 + dAngleValue;
				}
				else
				{
					dAngleValue	= (int)dAngleValue%360;
				}

				distance = ((dAngleValue - average)*(dAngleValue - average));
				sum += distance;
				break;

			case 4:
				count++;
				dAngleValue = dAngleValue - (iAnglePosition*90);
				if (dAngleValue<0)
				{
					dAngleValue = 360 + dAngleValue;
				}
				else
				{
					dAngleValue	= (int)dAngleValue%360;
				}

				distance = ((dAngleValue - average)*(dAngleValue - average));
				sum += distance;
				break;
		}  //switch
	}
	
	
	if(count > 0)
	{
		//fprintf(file_handle, "Maximum angle value: %f\n", findmax); 
		//fprintf(file_handle, "Minimum angle value: %f\n\n", findmin);  
		
		result = 3*sqrt(sum/count);
		fprintf(file_handle, "STDEV * 3 : %f\n", result);
		if(result < dRange1 || result > dRange2)  //range
		{
			//failed
			failResult = 1;
			fprintf(file_handle, "STDEV not between ranges ... FAILED!\n\n");
		}
		else
		{
			//succes 3*sigma between the ranges
			failResult = 0; 
			fprintf(file_handle, "STDEV  between ranges ... PASSED!\n\n");
		}

	}
	
	if (auxFailRes == 1)
	{
		failResult = 1;	
	}
	
	printCheckResults(file_handle, "CheckSTDEV" , failResult, errorPreSTDEV, errorIndivSTDEV, 0, 0, 0,result,dRange2,dRange1);	
	return failResult;

}

//MARVYN 28/07/2021 function that will find the name associate to the standard frame initial name 
char* StandardToAttribute(char *Param)
{

	char range[100] = "";
	char range2[50] = ""; 
	int index = 3;
	char globalName[100]="";
	char attributeName[100]="";
	int end =0;
	
 while (end == 0)
 {
	sprintf(range, "A%d", index); 
	ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, globalName);
	//printf("stand = _%s_ and attribute = _%s_\n",Param,globalName);
 	if (strcmp(Param,globalName) == 0)
	{
		end = 1;
		sprintf(range2, "B%d", index);
		ExcelRpt_GetCellValue (worksheetHandledata1,range2, CAVT_CSTRING, attributeName );
		return attributeName;
	}
	index++;
 }
 return 0;
	 
}

//MARVYN 29/07/2021 function that will find the name associate to the standard frame initial name 
char* DiagToAttribute(char *Param)
{

	char range[100] = "";
	char range2[50] = ""; 
	int index = 3;
	char globalName[100]="";
	char attributeName[100]="";
	int end =0;
	
 while (end == 0)
 {
	sprintf(range, "C%d", index); 
	ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, globalName);
	//printf("stand = _%s_ and attribute = _%s_\n",Param,globalName);
 	if (strcmp(Param,globalName) == 0)
	{
		end = 1;
		sprintf(range2, "D%d", index);
		ExcelRpt_GetCellValue (worksheetHandledata1,range2, CAVT_CSTRING, attributeName );
		return attributeName;
	}
	index++;
 }
 return 0;
	 
}
//MARVYN 29/07/2021
char* SW_IndentToAttribute(char *Param)
{

	char range[100] = "";
	char range2[50] = ""; 
	int index = 3;
	char globalName[100]="";
	char attributeName[100]="";
	int end =0;
	
 while (end == 0)
 {
	sprintf(range, "C%d", index); 
	ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, globalName);
	//printf("stand = _%s_ and attribute = _%s_\n",Param,globalName);
 	if (strcmp(Param,globalName) == 0)
	{
		end = 1;
		sprintf(range2, "D%d", index);
		ExcelRpt_GetCellValue (worksheetHandledata1,range2, CAVT_CSTRING, attributeName );
		return attributeName;
	}
	index++;
 }
 return 0;
	 
}

//MARVYN 30/07/2021
char* DruckToAttribute(char *Param)
{

	char range[100] = "";
	char range2[50] = ""; 
	int index = 3;
	char globalName[100]="";
	char attributeName[100]="";
	int end =0;
	
 while (end == 0)
 {
	sprintf(range, "I%d", index); 
	ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, globalName);
	//printf("stand = _%s_ and attribute = _%s_\n",Param,globalName);
 	if (strcmp(Param,globalName) == 0)
	{
		end = 1;
		sprintf(range2, "J%d", index);
		ExcelRpt_GetCellValue (worksheetHandledata1,range2, CAVT_CSTRING, attributeName );
		return attributeName;
	}
	index++;
 }
 return 0;
	 
}

//MARVYN 30/07/2021
char* LSEToAttribute(char *Param)
{

	char range[100] = "";
	char range2[50] = ""; 
	int index = 3;
	char globalName[100]="";
	char attributeName[100]="";
	int end =0;
	
 while (end == 0)
 {
	sprintf(range, "G%d", index); 
	ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, globalName);
	//printf("stand = _%s_ and attribute = _%s_\n",Param,globalName);
 	if (strcmp(Param,globalName) == 0)
	{
		end = 1;
		sprintf(range2, "H%d", index);
		ExcelRpt_GetCellValue (worksheetHandledata1,range2, CAVT_CSTRING, attributeName );
		return attributeName;
	}
	index++;
 }
 return 0;
	 
}



//Ajout MaximePAGES 18/08/2020
int CheckPreSTDEV(char * wuID, char *Value, char *sFrameNb, char *sAngleEmission, char *realFCParam, char *FunctionCodeValue, char *Parametertocheck, char *InterfacetoCheck)  //char *pathname2
{
	int boucle = 0, end = 1, endCondition = 1, zerofound = 0;
	
	char *CheckParamFC;
	char CKSGood[3] = "";
	char _ATimeColumn[255] = "";
	char _FunctionCode[50] = "";
	char range[30] = "";
	char rangeA[30] = "";
	char rangeFC[30] = "";
	char rangeFN[30] = "";
	char rangeAP[30] = ""; 
	char GoodSumRange[30] = "";
	char foundCellRange[30] = "";
	char FCRow[30] = "";
	char foundCellGoodSumRange[30] = "";
	char cellParamRange[30] = "";
	char cellFCRange[30] = "";
	char cellGoodSumRange[30] = "";
	char cellFrameNBRange[30]="";
	char cellAngle_position_Range[30] = "";
	char varParam[100]="";
	char CheckParam[255]= "/ROW/@";
	//char Rowparam[255] = 		"/ROW/@";  //for parameter to check
	char Rowparam2[255] = 		"/ROW/@";  //for FC
	char CheckGoodsum[100] = 	"/ROW/@";
	
	char Frame_number[150] = 	"/ROW/@";//for Frame number
	char Angle_position[100] =  "/ROW/@";
	char FrameNBRow[30] = "";
	char Angle_position_Row[30] = "";
	
	char iFrameNB[5] = "";
	char varFramNumber[5] = "";
	char varAnglePosition[5] = "";
	//char varAngle_position[30] = "";
	char auxAngleEmission[10] = "";
	int iAngleEmission = 0;
	 

	int failResult = 0;

	double dRange1 = 0.0, dRange2;

	int CKSGoodvalid = 0;
	int CKSGoodinvalid = 0;
	
	char CheckWUID[30] = 		"/ROW/@"; 
	char _WUIDColumn[255] = "";
	char WUIDRange[30] = ""; 
	char cellWUIDRange[30] = ""; 
	char WUID[30] = ""; 
	
	char CheckAngleDetection[30] = 		"/ROW/@";
	char _AngleDetectionColumn[255] = "";
	char AngleDetectionRange[30] = ""; 
	char cellAngleDetectionRange[30] = ""; 
	char AngleDetection[30] = ""; 
	
	int noLab=0;
	int count = 1;
	char scount[10] ="";
	
	int zeroFound = 0;
	int oneFound = 0; 
	int twoFound = 0; 
	int threeFound = 0; 
	int fourFound = 0;
	int anglePosCounter = 0;
	
	//MARVYN 27/07/2021
	 
	strcat(CheckGoodsum,translateData(InterfacetoCheck, "CKS_Validity"));
	strcat(CheckWUID,translateData(InterfacetoCheck,"ID")); 
	strcat(Frame_number,translateData(InterfacetoCheck, "FrameNb"));
	strcat(Angle_position,translateData(InterfacetoCheck,"Angle_Position"));
	strcat(CheckParam,translateData(InterfacetoCheck,"Angle")); 
	strcat(CheckAngleDetection,translateData(InterfacetoCheck,"LSE_Flag"));          
	//printf("CheckWUID = %s\n",CheckWUID);
	//printf("CheckGoodsum = %s\n",CheckGoodsum); 
	//printf("Frame_number = %s\n",Frame_number);
	//printf("Angle_position = %s\n",Angle_position);
	//printf("CheckAngleDetection = %s\n",CheckAngleDetection);
	
	//angle value 0 - 360
	RetrieveandSeparateRange(Value); // 0-4;
	dRange1 = atof(range1); //range1 and range2 are global variables
	dRange2 = atof(range2);

	//head of the check
	fprintf(file_handle, "\n****************CheckSTDEV****************\n\n");
	//fprintf(file_handle, "                 Check_%d\n", NBTestCheck);
	fprintf(file_handle, "PARAMETERS AND PRE-TEST ***\n\n");

	CheckParamFC = strcat(Rowparam2, realFCParam); //EX: /ROW/@Function_code

	//check param
	//CheckParam = strcat(Rowparam, Parametertocheck);  //Ex CheckParam = /ROW/@Channel
	//take excel file

	ExcelRpt_GetWorksheetFromIndex(workbookHandle9, 1, &worksheetHandle9);  //Worksheet Configuration

	// 1 - looking for "/ROW/@CKSGood"
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckGoodsum, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,foundCellGoodSumRange);
	char caracter1[] = "$";  			//foundCellRange =  $P$2
	char *token1 = strtok(foundCellGoodSumRange, caracter1);   //token1 = C
	if(token1 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckGoodsum);
	else
		strcpy(cellGoodSumRange, token1);

	//2 - looking for "/ROW/@Function_code" = $W$2
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckParamFC, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,FCRow);
	char caracter2[] = "$";
	char *token2 = strtok(FCRow, caracter2);
	if(token2 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckParamFC);
	else
		strcpy(cellFCRange, token2); 		// cellFCRange = W2

	//3 - looking for "/ROW/@Frame_number" = $U$2
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, Frame_number, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,FrameNBRow);
	char caracter3[] = "$";
	char *token3 = strtok(FrameNBRow, caracter3);
	if(token3 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", Frame_number);
	else
	{
		strcpy(cellFrameNBRange, token3); 	// cellFCRange = U2
		strcpy(iFrameNB, sFrameNb); 		//iFrameNB = "2"
	}

	//4 - looking for "/ROW/@Angle_position" = $F$2
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, Angle_position, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,Angle_position_Row);
	char caracter4[] = "$";
	char *token4 = strtok(Angle_position_Row, caracter4);
	if(token4 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", Angle_position);
	else
	{
		strcpy(cellAngle_position_Range, token4); 	// cellAngle_position_Range = F2
		strcpy(auxAngleEmission, sAngleEmission);
		iAngleEmission = atoi(auxAngleEmission);//iAngleEmission = "4" or 1 or 2
	}

	//5 - looking for "/ROW/@parameter"
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckParam, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,foundCellRange);
	char caracter5[] = "$";  //EX: foundCellRange =  $M$2
	char *token5 = strtok(foundCellRange, caracter5);
	if(token5 == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckParam);
	else
		strcpy(cellParamRange, token5);   // cellParamRange = M2


	//6 - looking for "/ROW/@ID" = $W$2 // the WU ID
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckWUID, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,WUID);
	char caracterWUID[] = "$";
	char *tokenWUID = strtok(WUID, caracterWUID);
	if(tokenWUID == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckWUID);
	else
		strcpy(cellWUIDRange, tokenWUID);
	
	//6 - looking for "/ROW/@Angle_detection" = $E$2 // the Angle detection
	ExcelRpt_Find(worksheetHandle9,CAVT_CSTRING, CheckAngleDetection, "A1",ExRConst_Values, ExRConst_Whole, ExRConst_ByRows,ExRConst_Next ,1,0,AngleDetection);
	char caracterAngleDetection[] = "$";
	char *tokenAngleDetection = strtok(AngleDetection, caracterAngleDetection);
	if(tokenAngleDetection == NULL)
		fprintf(file_handle, "An error has occured\nThe column %s was not found!\n\n", CheckAngleDetection);
	else
		strcpy(cellAngleDetectionRange, tokenAngleDetection);
	
	

	if(token1 != NULL && token2 != NULL && token3 != NULL && token4 != NULL && token5 != NULL && tokenWUID != NULL && tokenAngleDetection != NULL)
	{
		
		if (lab2 == 0)
		{
			if (lab1 == 0)
			{
			fprintf(file_handle, "Between %s (%d ms) and %s (%d ms)\n", 	label1, Time1, label2, Time2);
			}
			else
			{
			fprintf(file_handle, "Between the beginning and %s (%d ms)\n", label2, Time2);
			}
		}
		else
		{
		   	if (lab1 == 0)
			{
			fprintf(file_handle, "Between %s (%d ms) and the end of the script\n", 	label1, Time1);
			}
			else
			{
			fprintf(file_handle, "Between the beginning and the end of the script\n");
			}
		}
		fprintf(file_handle, "Function code: %s\n",FunctionCodeValue);
		fprintf(file_handle, "Frame number: %s\n", iFrameNB);
		fprintf(file_handle, "Angle emission: %d\n\n", iAngleEmission);
		fprintf(file_handle, "Range1 = %f and Range2 = %f\n\n",dRange1, dRange2 );
		//vefirifier  entre labels
		//parcourir column to find time
		for(boucle=2; end==1; boucle++)
		{
			//until find label1
			sprintf(rangeA, "A%d", boucle+1);
			ExcelRpt_GetCellValue (worksheetHandle9, rangeA, CAVT_CSTRING, _ATimeColumn);

			if(strcmp(label1, _ATimeColumn) == 0 || lab1==1)
			{
				//analyse until find label 2
				for(int i = boucle+1 ; endCondition == 1; i++)
				{
					//1 - check Goodsum first
					sprintf(GoodSumRange,"%s%d",cellGoodSumRange, i); //P3
					ExcelRpt_GetCellValue (worksheetHandle9, GoodSumRange, CAVT_CSTRING, CKSGood);

					if(strcmp(CKSGood, "1") == 0)
					{
						
						// we look for the good WU ID
						sprintf(WUIDRange,"%s%d",cellWUIDRange, i);
						ExcelRpt_GetCellValue (worksheetHandle9, WUIDRange, CAVT_CSTRING, _WUIDColumn);

						if(strcmp(_WUIDColumn, wuID) == 0)
						{
							CKSGoodvalid++ ;
							//2 - check FC second
							sprintf(rangeFC,"%s%d",cellFCRange, i);
							ExcelRpt_GetCellValue (worksheetHandle9, rangeFC, CAVT_CSTRING, _FunctionCode);

							if(strcmp(_FunctionCode, FunctionCodeValue) == 0)
							{
								//3 - Check Frame Number
								sprintf(rangeFN,"%s%d",cellFrameNBRange, i);
								ExcelRpt_GetCellValue (worksheetHandle9, rangeFN, CAVT_CSTRING, varFramNumber);
								if(strcmp(varFramNumber, iFrameNB) == 0)
								{
									// - Check Angle Detection
									sprintf(AngleDetectionRange,"%s%d",cellAngleDetectionRange, i);
									ExcelRpt_GetCellValue (worksheetHandle9, AngleDetectionRange, CAVT_CSTRING, _AngleDetectionColumn);
									if(strcmp(_AngleDetectionColumn, "1") == 0)
									{
										
										// we save the Angle Position
										sprintf(rangeAP,"%s%d",cellAngle_position_Range, i);
										ExcelRpt_GetCellValue (worksheetHandle9, rangeAP, CAVT_CSTRING, varAnglePosition);
										strcpy(myFrameTab[count].anglepos,varAnglePosition);


										//count number of angle position
										if ( (strcmp(varAnglePosition,"0") == 0 ) && (zeroFound == 0)  )
										{
										   zeroFound = 1;
										   anglePosCounter++;
										}
										if ( (strcmp(varAnglePosition,"1") == 0 ) && (oneFound == 0)  )
										{
										   oneFound = 1;
										   anglePosCounter++;
										}
										if ( (strcmp(varAnglePosition,"2") == 0 ) && (twoFound == 0)  )
										{
										   twoFound = 1;
										   anglePosCounter++;
										}
										if ( (strcmp(varAnglePosition,"3") == 0 ) && (threeFound == 0)  )
										{
										   threeFound = 1;
										   anglePosCounter++;
										}
										if ( (strcmp(varAnglePosition,"4") == 0 ) && (fourFound == 0)  )
										{
										   fourFound = 1;
										   anglePosCounter++;
										}
									
									
										// we save the Angle Value  
										sprintf(range,"%s%d",cellParamRange, i);
										ExcelRpt_GetCellValue (worksheetHandle9, range, CAVT_CSTRING, varParam);
										strcpy(myFrameTab[count].angleval,varParam); 
									
										count++;
									}
								}  // Frame Number
							}  //check FC second

						}

					} //Goodsum
					if(strcmp(CKSGood, "0") == 0)
					{
						CKSGoodinvalid++ ;
						zerofound = 1;
						fprintf(file_handle, "CKSGood: 0 was found in line %d\n", i);
					}


					sprintf(rangeA, "A%d", i);
					ExcelRpt_GetCellValue (worksheetHandle9, rangeA, CAVT_CSTRING, _ATimeColumn);
					if(strcmp(label2, _ATimeColumn) == 0)   //stop condition check
					{
						if (noLab == 1 || FirstLab==1  || TwoLabs==1)
						{
						if(zerofound == 1)
						{
							float percentageCKSGood = ((double)CKSGoodinvalid/( (double)CKSGoodinvalid+ (double)CKSGoodvalid))*100.0;
							fprintf(file_handle, "\n%.2f%% of the CKSGood are equal to 0!\n\n",percentageCKSGood );
						}

						endCondition = 0; //stop analysis
						end = 0;
						
						//Average = sum/count;
						if(count > 1)
						{
						  strcpy(myFrameTab[0].anglepos,"nb");
						  sprintf(scount,"%d",count-1);
						  strcpy(myFrameTab[0].angleval,scount);
						  
						  //check nb of angle position 
						  if (anglePosCounter == iAngleEmission) 
						  {
							fprintf(file_handle, "There are %d angle position(s) detected, as expected (%d).\n\n",anglePosCounter,iAngleEmission);  
						  }
						  else
						  {
							failResult = 1; 
							fprintf(file_handle, "There are %d angle position(s) detected, different from the %d expected.\n\n",anglePosCounter,iAngleEmission);
						  }
						  
						  
						}
						else
						{
							failResult = 1;
							fprintf(file_handle, "There are no emissons between the labels.\n\n");
						}
						}
						noLab=1;
					} 
				} 
			} 
		}
		end=1;
		
	}
	else
	{
		fprintf(file_handle, "The CheckPreSTDEV was not performed!\n\n");
		failResult =  -1;
	}

	return failResult;

}


//***********************************************************************************************
// Modif - MaximePAGES 27/08/2020
// This mode shows 
//***********************************************************************************************
int CVICALLBACK mode_analyse (int panel, int control, int event,
							   void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{
			//	enable_create = 1;
		case EVENT_COMMIT:
			
			mode = 0;
			
			SetCtrlVal(GiPanel,PANEL_MODE_MODE_CREATION,0);
			SetCtrlVal(GiPanel,PANEL_MODE_MODE_EXE,0); 
			SetCtrlVal(GiPanel,PANEL_MODE_MODE_ANALYSE,1); 
			
			//Make visible tables : Script sequence and Test Script 
			SetCtrlAttribute (GiPanel, PANEL_MODE_EXP_RESULTS, 		ATTR_VISIBLE ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_SCRIPTS_DETAILS, 	ATTR_VISIBLE ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_DECORATION_2, 		ATTR_VISIBLE ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_DECORATION_3, 		ATTR_VISIBLE ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_SCRIPTS_DETAILS_2, 	ATTR_VISIBLE ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_TABLE_SCRIPT, 		ATTR_VISIBLE ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_PICTURE, 			ATTR_VISIBLE ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_DECORATION_5, 		ATTR_VISIBLE ,1);
			SetCtrlAttribute (GiPanel, PANEL_MODE_SHOW_ACC, 		ATTR_DIMMED ,1);
			SetCtrlAttribute (GiPanel, PANEL_MODE_SHOW_PRESS, 		ATTR_DIMMED ,1);
			SetCtrlAttribute (GiPanel, PANEL_MODE_DECORATION_8, 		ATTR_DIMMED ,1);
			SetCtrlAttribute (GiPanel, PANEL_MODE_TEXTMSG_9, 		ATTR_DIMMED ,1);
			SetCtrlAttribute (GiPanel, PANEL_MODE_TEXTMSG_8, 		ATTR_DIMMED ,1);
			SetCtrlAttribute (GiPanel, PANEL_MODE_DECORATION_6, 		ATTR_VISIBLE ,1);
			SetCtrlAttribute (GiPanel, PANEL_MODE_EXPRESULTSC, 		ATTR_VISIBLE ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_TESTSCRIPT, 			ATTR_VISIBLE ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_SCRIPTSEQEXE, 		ATTR_VISIBLE ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_ONLINESEQ, 			ATTR_VISIBLE ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_ENDTIME, 			ATTR_VISIBLE ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_BUT_START_RUN,  		ATTR_DIMMED, 1);
			SetCtrlAttribute (GiPanel, PANEL_MODE_DECORATION_7,  		ATTR_DIMMED, 1);
			SetCtrlAttribute (GiPanel, PANEL_MODE_TOTALTIME, 			ATTR_VISIBLE ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_ENDTIMESEQUENCE, 	ATTR_VISIBLE ,0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_ENDTIMESEQUENCE, 	ATTR_VISIBLE ,0);
			SetCtrlAttribute(GiPanel, PANEL_MODE_PROGRESSBAR, 		ATTR_VISIBLE, 0);
			SetCtrlAttribute(GiPanel, PANEL_MODE_PROGRESSBARSEQ, 		ATTR_VISIBLE, 0);
			SetCtrlAttribute (GiPanel, PANEL_MODE_SAVEFILEBUTTON, ATTR_DIMMED, 	1); //AJOUT MaximePAGES - 15/06/2020
			
			
			SetCtrlAttribute (GiPanel, PANEL_MODE_INDICATOR_TESTSCRIPT, 	ATTR_VISIBLE ,1);  
			SetCtrlAttribute (GiPanel, PANEL_MODE_INDICATOR_TESTFOLDER, 	ATTR_VISIBLE ,1);  
			SetCtrlAttribute (GiPanel, PANEL_MODE_BUTTON_TESTSCRIPT, 	ATTR_VISIBLE ,1);  
			SetCtrlAttribute (GiPanel, PANEL_MODE_BUTTON_TESTFOLDER, 	ATTR_VISIBLE ,1);   
			SetCtrlAttribute (GiPanel, PANEL_MODE_BUTTON_ANALYSE, 	ATTR_VISIBLE ,1);
			SetCtrlAttribute (GiPanel, PANEL_MODE_TEXTBOX_ANALYSIS, 	ATTR_VISIBLE ,1); 
			SetCtrlAttribute (GiPanel, PANEL_MODE_CHECKRESULTS, 	ATTR_VISIBLE ,1);  
			
			
			

			break;
	}
	return 0;
}

//***********************************************************************************************
// Modif - MaximePAGES 27/08/2020
// Button to load test scipt in Analyse Mode
//***********************************************************************************************
int CVICALLBACK button_testscript (int panel, int control, int event,
								 void *callbackData, int eventData1, int eventData2)
{
	char sProjectDir[MAX_PATHNAME_LEN];
	int iRet;
	int curseur_chaine2=0;
	int compteur_chaine2=0;
	int fin_chaine2=0;
	char testscript_name[MAX_PATHNAME_LEN]="";
	

	
	





	// Formation du chemin vers le répertoire des configurations
	GetProjectDir(sProjectDir);

	switch (event)
	{
		case EVENT_COMMIT:

		
			iRet = FileSelectPopupEx (sProjectDir, "*.xlsx", "", "", VAL_SELECT_BUTTON, 0, 1, GsTestScript_Analyse);
			
			
		
			if(iRet > 0)
			{
				testCaseLoaded = 1;
				
				if (logsxmlLoaded == 1)
				{
					SetCtrlAttribute(GiPanel, PANEL_MODE_BUTTON_ANALYSE, ATTR_DIMMED, 0);
						
				}
				else 
				{
					SetCtrlAttribute(GiPanel, PANEL_MODE_BUTTON_ANALYSE, ATTR_DIMMED, 1);  
				}
				

				//Récupération du nom de la database
				for(curseur_chaine2=strlen(GsTestScript_Analyse); fin_chaine2==0 ; curseur_chaine2--)
				{
					if( GsTestScript_Analyse[curseur_chaine2]=='\\' )
					{
						fin_chaine2=1;
						for( compteur_chaine2=0; (curseur_chaine2+compteur_chaine2) < strlen(GsTestScript_Analyse) ; compteur_chaine2++)
						{
							testscript_name[compteur_chaine2]=GsTestScript_Analyse[curseur_chaine2+compteur_chaine2+1];
						}
					}
				}

			   	
				//display of the archive database - a green led inform that an archive was loaded
				
				//SetCtrlVal (panel, PANEL_MODE_LED22, 0x0006B025); 
				SetCtrlVal (panel, PANEL_MODE_INDICATOR_TESTSCRIPT, testscript_name);
				strcpy(GsTestScriptName_Analyse,testscript_name);
				


			}
			else //0 VAL_NO_FILE_SELECTED
			{
				testCaseLoaded = 0;    
				strcpy(testscript_name, "");
				strcpy(GsTestScript_Analyse, "");
				SetCtrlVal(panel,PANEL_MODE_INDICATOR_TESTSCRIPT,"");
				SetCtrlAttribute(GiPanel, PANEL_MODE_BUTTON_ANALYSE, ATTR_DIMMED, 1);  
			
				//SetCtrlVal (panel, PANEL_MODE_LED22, 0x00000000);
				
				return 0;
			}


			break;
	}

	return 0;
}

//***********************************************************************************************
// Modif - MaximePAGES 27/08/2020
// Button to load test folder in Analyse Mode
//***********************************************************************************************
int CVICALLBACK button_testfolder (int panel, int control, int event,
								 void *callbackData, int eventData1, int eventData2)
{

	int curseur_chaine2=0;
	
	
	char testfolder_name[MAX_PATHNAME_LEN]="";
	
	char defaultDir[MAX_PATHNAME_LEN] = "D:\\";
	char sProjectDir[MAX_PATHNAME_LEN];
	int iRet = 0, errDir=0;

	errDir = GetProjectDir(sProjectDir);
	if(errDir == -1 || errDir == -2)
		strcpy(sProjectDir, defaultDir);

	switch (event)
	{
		case EVENT_COMMIT:

		
			iRet = FileSelectPopupEx (sProjectDir, "*.xml", "", "", VAL_SELECT_BUTTON, 0, 1, GsTestFolder_Analyse); 
			
			if(iRet > 0)   
			{
				
				logsxmlLoaded = 1;
				
				if (testCaseLoaded == 1)
				{
					SetCtrlAttribute(GiPanel, PANEL_MODE_BUTTON_ANALYSE, ATTR_DIMMED, 0);
						
				}
				else 
				{
					SetCtrlAttribute(GiPanel, PANEL_MODE_BUTTON_ANALYSE, ATTR_DIMMED, 1);  
				}
				
				
				
				SetCtrlVal (panel, PANEL_MODE_INDICATOR_TESTFOLDER, GsTestFolder_Analyse); 
				
				curseur_chaine2=strlen(GsTestFolder_Analyse) ;
				for(int i= curseur_chaine2-9 ; i < curseur_chaine2+1 ; i++)
				{
					GsTestFolder_Analyse[i]= '\0';
				}

				//display of the archive database - a green led inform that an archive was loaded
				
				//SetCtrlVal (panel, PANEL_MODE_LED32, 0x0006B025); 
				
				
			}
			else
			{
				logsxmlLoaded = 0; 
				SetCtrlAttribute(GiPanel, PANEL_MODE_BUTTON_ANALYSE, ATTR_DIMMED, 1);
				
				strcpy(testfolder_name, "");
				strcpy(GsTestFolder_Analyse, "");
				SetCtrlVal(panel,PANEL_MODE_INDICATOR_TESTFOLDER,"");
				//SetCtrlVal (panel, PANEL_MODE_LED12, 0x00000000); 
				return 0;
			}
			break;
	}
	return 0;
}


//***********************************************************************************************
// Modif - MaximePAGES 27/08/2020
// Button to load test scipt in Analyse Mode
//***********************************************************************************************

int CVICALLBACK button_analyse (int panel, int control, int event,
						   void *callbackData, int eventData1, int eventData2)
{

	char newExeTestXls[MAX_PATHNAME_LEN];
	char * newCurrentTestName;
													  
	
	
	
	char newfolder[MAX_PATHNAME_LEN];
	
	int oldValue;
	SYSTEMTIME t;

	switch (event)
	{
		case EVENT_COMMIT:

			ongoingAnalyse = 1;
			ResetTextBox (GiPanel, PANEL_MODE_TEXTBOX_ANALYSIS, ""); 
			SetCtrlAttribute(GiPanel, PANEL_MODE_CLOCK_ANALYSE, ATTR_VISIBLE, 1); 
			//new folder
			//Init interface
			GetLocalTime(&t); 
			newCurrentTestName = strtok(GsTestScriptName_Analyse,".");
			
			sprintf(newfolder,"%s\\%s_%02d-%02d-%d_%02d-%02d-%02d",GsLogPathInit,newCurrentTestName,t.wDay,t.wMonth,(t.wYear-2000),t.wHour,t.wMinute,t.wSecond);
			oldValue = SetBreakOnLibraryErrors (0);
			MakeDir(newfolder);  
			SetBreakOnLibraryErrors (oldValue);
			
			
			//Test script translation 
			sprintf(newExeTestXls,"%s\\%s_t.xlsx",newfolder,newCurrentTestName);
			translateScriptExcel(GsTestScript_Analyse,newExeTestXls);
			
			//Analyse
			openingExpectedResults(newExeTestXls, GsTestFolder_Analyse,newfolder);

			SetCtrlAttribute(GiPanel, PANEL_MODE_CLOCK_ANALYSE, ATTR_VISIBLE, 0);  
			ongoingAnalyse = 0;
			break;
	}
	return 0;
}


//Marvyn 12/07/2021
//Read the config file to load it after 
char* configInit(char type)
{
	FILE* fichier = NULL;
	chaine=malloc(1000*sizeof(char));;
	char *token=malloc(1000*sizeof(char));
	fichier = fopen("../initial_configurations.txt","r");
	int counter = 0;
	  if (fichier != NULL)
    {
        while(fgets(token,1000,fichier)!=NULL)
		{   
			strtok(token,"=");
			if ( token[0] == type && token[1] != ':' )
			{
			//printf("type =%c\n",type);
			while( token != NULL )
			{
				strcpy(chaine,token);
				token=strtok(NULL, "=");
				counter++;
			}
			//printf("%s\n",chaine);
			free(token);
			token=malloc(1000*sizeof(char));
			}
		}
		
    }
    else
    {
        // On affiche un message d'erreur si on veut
        //printf("Impossible to open the file");
    }
    free(token);
	fclose(fichier);
	//printf("%s\n",chaine);
	if (counter >1)
	{
	return chaine;
	}
	else
	{
	chaine = "";
	return chaine ;
	}
}

//MARVYN
char* configName(char *path)
{
//	char *token=malloc(300*sizeof(char));
	char name[1000]="";
	strtok(path,"\\");
	while (path != NULL)
	{
		strcpy(name,path);
		path=strtok(NULL,"\\");
		
	}
//	free(token);
	return name;
}

//MARVYN
/*char* GetColumn(char *interface)
{
	char *Alpahbet="ABCDEFGHIJKLMNOPQRSTUVWXYZ"  
	int i = 0;
	int end =0;
	char* attName="";
	while( end == 0 || i=26)
	{
		sprintf(range, "%s%d",Alphabet[i], index);	
		ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, attName);
		if (strcmp(attName,interface) == 0)
		{
			end=1;
		}
		else
		{
		i++;
		}
	}
	return Alphabet[i];
}	  */
		   
//MARVYN 02/07/2021
int GetALLFramesTypes()  
{
	char Alphabet[30][30]={"A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z","AA","AB","AC","AD","AE","AF","AG","AH","AI","AJ"}; 
	int i = 0;
	char range[20] ;
	char attName[100];
	int end =0;
	char *aux="";
	
	while ( end == 0)
	{
		sprintf(range, "%s2",Alphabet[i]);	
		ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, attName);
		if (strcmp(attName,"Interface") == 0)
		{
			sprintf(range, "%s2",Alphabet[i+1]);	
			ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, attName);
			strcpy(Interfaces[indexInterfaceStruct].columnParam,Alphabet[i]); 
			strcpy(Interfaces[indexInterfaceStruct].columnAttribute,Alphabet[i+1]);
			strcpy(Interfaces[indexInterfaceStruct].name,attName) ;
		    //printf("Interfaces[%d].name = %s and index = %d\n",0,Interfaces[0].name,indexInterfaceStruct); 
			indexInterfaceStruct++;
		}
		else if (strcmp(attName,"") == 0)
		{
			sprintf(range, "%s2",Alphabet[i+1]);	
			ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, attName);
			if (strcmp(attName,"") == 0)
			{
				end = 1;
			}
		} 
		i++;
	}
	//printf("Interfaces[%d].name = %s\n",0,Interfaces[0].name);
	return 0 ;
	
}



		// DESC pour récupérer le nom exact et différencier les trames ?? Autrte tableau ???
/*
struct interface
{
 char columnParam ;
 char columnAttribute ; 
 char *name ;
}
*/

int CheckRFProtocol(char *InterfaceToCheck,char *RFprot)
	{
		char DescParam[100];
		
		for (int i=0;i<indexInterfaceStruct;i++)
		{
			if (strcmp(Interfaces[i].name,InterfaceToCheck) == 0)
			{
				strcpy(DescParam,translateData(InterfaceToCheck,"Desc"));
				
				if ( strstr(DescParam,RFprot) != NULL )
				{
					return 1;
				}
			}
		}
		
		return 0;	
	}

int findColumn(char *name)
{
	char Alphabet[30][30]={"A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z","AA","AB","AC","AD","AE","AF","AG","AH","AI","AJ"}; 
	int i = 0;
	char range[20] ;
	char attName[100];
	int end =0;

	while ( end == 0)
	{
		sprintf(range, "%s1",Alphabet[i]);	
		ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, attName);
		if (strcmp(attName,name) == 0)
		{
		 strcpy(col.columnParam,Alphabet[i]);
		 strcpy(col.columnAttribute,Alphabet[i+1]);
		 
		 return 1;	
		}
		else if (strcmp(attName,"") == 0)
		{
			sprintf(range, "%s1",Alphabet[i+1]);	
			ExcelRpt_GetCellValue (worksheetHandledata1, range, CAVT_CSTRING, attName);
			if (strcmp(attName,"") == 0)
			{
				end = 1;
			}
		} 
		i++;
	}
	//printf("Interfaces[%d].name = %s\n",0,Interfaces[0].name);
	return 0;	
	
	
}
