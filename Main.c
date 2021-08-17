#include "IhmModes.h"

// DEBUT TABLE
//******************************************************************************
// PRO: BANC DE TEST LSE
// PRG: Main.C
// VER: Version I00A00D du 04/01/2011
// AUT: C. BERENGER / MICROTEC
//
// ROL: Programme principal du banc de test LSE
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
//******************************************************************************
//
//******************************************************************************
// FIN TABLE

//******************************************************************************
// Inclusions
//******************************************************************************

// Librairies standards
#include "MonWin.h"
#include <cvidotnet.h>
#include "DotNetDll.h"
#include <cvintwrk.h>
#include <utility.h>
#include <ansi_c.h>
#include <userint.h>
#include <cvirte.h>
#include <windows.h>
#include <cviauto.h>
#include "excelreport.h"
#include "progressbar.h"
#include "toolbox.h"
#include <NIDAQmx.h>
#include <cvidef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// Librairies FOURNISSEUR

// Librairies MICROTEC
//#include "MessageIhm.h"
#include "ZoneMessage.h"

// Librairies spécifiques à l'application
#include "IhmAccueil.h"
#include "GstFiles.h"
#include "Modes.h"
#include "Usb6008.h"
#include "Variateur.h"
#include "Simulation.h"

#include "IPC.h"
void* hDLL=NULL;                // Instance of DLL
TAPI      API;

//******************************************************************************
// Définition de constantes
//******************************************************************************
//----- Options -------
//#define bSIMULATION
//---------------------

#define iNB_ESSAIS_MAX 			10
#define fMIN_VITESSE_INIT 		1
#define dMIN_PRESSION_SECURITE  0.2

#define sNOM_IHM_PRINCIPALE 	"IhmAccueil.uir"
#define sREP_ECRANC  			"Ecrans"
#define sCONFIG_FILE 			"Config.ini"
#define sTRADUC_FILE 			"MessageIhm.ini"
#define sREP_CONFIG  			"Config"
#define sNOM_LIBRAIRIE			"Main"


#define iCOM_VARIAT				1
#define iVIT_COM_VARIAT			19200

// Nom de la section pour les traduction de l'IHM
#define sSECTION_ACCUEIL		"IhmAccueil"

#define sOP_MODE_ACCUEIL	 	"Menu"  //"Operating Mode"  // Modif MaximePAGES 08/09/2020
#define sAUTO_MODE_ACCUEIL	 	"Automatic Mode"
#define sMANU_MODE_ACCUEIL	 	"Manual Mode"
#define sQUIT_ACCUEIL		 	"Quit"
#define sABOUT_ACCUEIL		 	"About"
#define sAPPLI_TITLE_ACCUEIL 	"Wheel Unit Testing Tool 2.0"
#define sRESET_BUTT_ACCUEIL	 	"Bench Reset"
#define sMESSAGES_ACCUEIL	 	"Messages"
#define sHARDWARE_INIT			"Hardware initialization..."
#define sERR_INIT_USB6008		"=> ERROR initializing usb6008 module!"
#define sINIT_USB6008_OK	 	"Init usb6008 module OK."
#define sERR_INIT_ENGINE	 	"=> ERROR initializing engine!"
#define sINIT_ENGINE_OK		 	"Init engine OK."
#define sERR_CTRL_ENGINE_FAN 	"=> ERROR controling fan engine!"
#define sSTOP_ENGINE_FAN_OK	 	"Stop fan engine OK."
#define sERR_CTRL_ELEC_LOCK	 	"=> ERROR controling door opener!"
#define sCMD_ELEC_LOCK_OK	 	"Command door opener OK."
#define sDOOR_1_OPENED		 	"--> Door 1 is opened..."
#define sDOOR_1_CLOSED		 	"Door 1 is closed."
#define sDOOR_2_OPENED		 	"--> Door 2 is opened..."
#define sDOOR_2_CLOSED		 	"Door 2 is closed..."
#define sSAFETY_HOOD_OPENED	 	"--> Safety hood is opened..."
#define sSAFETY_HOOD_CLOSED	 	"Safety hood is closed."
#define sSECUR_LOOP_OPENED	 	"--> Security loop is opened..."
#define sSECUR_LOOP_CLOSED	 	"Security loop is closed."
#define sEMERGENCY_STOP_ACTIV 	"--> Emergency stop is active..."
#define sEMERG_STOP_INACTIV	 	"Emergency stop is inactive."
#define sCAPT_VIB_ACTIV			"Vibrations detected!"
#define sCAPT_VIB_INACTIV		"Sensor vibrations OK."
#define sERR_READING_SECUR_STAT "=> ERROR reading security status!"
#define sERR_SPEED_TOO_HIGH	 	"=> ERROR: Speed engine is too high: %.03f trs/min!"
#define sRESET_PRESS		 	"Reset pressure... Please, wait..."
#define sERR_PRESS_TOO_HIGH	 	"=> ERROR: Pressure is too high: %.03f bars!"
#define sERR_READ_PRESS		 	"=> ERROR reading presure!"
#define sERR_USB6008_COMM	 	"=> USB6008 ERROR!"
#define sERR_VARIATOR		 	"=> Variator ERROR!"
#define sEND_HARD_INIT		 	"End Hardware initialization."
#define sMSG_WARNING_QUIT	 	"Are you sure you want to quit? All unsaved data will be lost..."
#define sERR_TRY_TO_STOP_ENGINE "Try to stop engine..."


// Répertoire de sauvegarde des traces
#define sSOUS_REP_TRACE			"Trace"

// Indice des éléments à traduire
#define iOP_MODE_ACCUEIL		0
#define iAUTO_MODE_ACCUEIL		1
#define iMANU_MODE_ACCUEIL		2
#define iQUIT_ACCUEIL			3
#define iABOUT_ACCUEIL			4
#define iAPPLI_TITLE_ACCUEIL	5
#define iRESET_BUTT_ACCUEIL		6
#define iMESSAGES_ACCUEIL		7

// Messages
#define iHARDWARE_INIT			8
#define iERR_INIT_USB6008		9
#define iINIT_USB6008_OK		10
#define iERR_INIT_ENGINE		11
#define iINIT_ENGINE_OK			12
#define iERR_CTRL_ENGINE_FAN	13
#define iSTOP_ENGINE_FAN_OK		14
#define iERR_CTRL_ELEC_LOCK		15
#define iCMD_ELEC_LOCK_OK		16
#define iDOOR_1_OPENED			17
#define iDOOR_1_CLOSED			18
#define iDOOR_2_OPENED			19
#define iDOOR_2_CLOSED			20
#define iSAFETY_HOOD_OPENED		21
#define iSAFETY_HOOD_CLOSED		22
#define iSECUR_LOOP_OPENED		23
#define iSECUR_LOOP_CLOSED		24
#define iEMERGENCY_STOP_ACTIV	25
#define iEMERG_STOP_INACTIV		26
#define iERR_READING_SECUR_STAT	27
#define iERR_SPEED_TOO_HIGH		28
#define iRESET_PRESS			29
#define iERR_PRESS_TOO_HIGH		30
#define iERR_READ_PRESS			31
#define iERR_USB6008_COMM		32
#define iERR_VARIATOR			33
#define iEND_HARD_INIT			34
#define iMSG_WARNING_QUIT		35
#define iERR_TRY_TO_STOP_ENGINE	36



//******************************************************************************
// Définition de types et structures
//******************************************************************************

//******************************************************************************
// Variables globales
//******************************************************************************
char GsPathApplication[MAX_PATHNAME_LEN];
int GiPanelMain			= 0;
int GiPanelAbout		= 0;
volatile int GbQuitterMain	= 0;
int GbUsb6008OpenComOk 	= 0;
int GbVariatOpenComOk 	= 0;
stConfig GstConfig;
HANDLE  GhDLL;
//----- Modif du 30/11/2010 par C.BERENGER	------ DEBUT ------
int GiCpt				= 0;
static int GiThreadPoolHandle1;
static int GiThreadFctId1;
//----- Modif du 30/11/2010 par C.BERENGER	------ FIN ------
WuAutoCheck_WU_Automation_Test_and_Folder_Manager instanceLseNet;
extern WuAutoCheck_WU_Automation_Test_and_Folder_Manager instanceLseNetModes;
extern TAPI GapiANum;
//Initialize_WindowsFormsApplication2();

//******************************************************************************
// Fonctions internes au source
//******************************************************************************

//******************************************************************************
// Corps des fonctions et procédures internes au source
//******************************************************************************

 char testtab[255][255] = {"",""} ;

// DEBUT ALGO
//***************************************************************************
// void AddMessageZM(char *sMsg)
//***************************************************************************
//  - sMsg: message
//
//  - Fonction ajoutant un message dans la Zone de Message
//
//  - Aucun
//***************************************************************************
// FIN ALGO
void AddMessageZM(char *sMsg)
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
		*ptMgs='\0';
		ptMgs++;
		*ptMgs='\0';
		ptMgs++;
		InsertTextBoxLine (GiPanelMain, PANEL_MESSAGES,-1,ptMgsCopie);
		ptMgsCopie=ptMgs;
	}
	if (ptMgsCopie!=NULL) InsertTextBoxLine (GiPanelMain,PANEL_MESSAGES,-1,ptMgsCopie);
	GetNumTextBoxLines (GiPanelMain, PANEL_MESSAGES, &iNbLignes);
	SetCtrlAttribute (GiPanelMain, PANEL_MESSAGES, ATTR_FIRST_VISIBLE_LINE, iNbLignes);
	// A chaque message ajouté on vérifie s'il faut purger la ZM
	ZoneMessageSaturee(GiPanelMain,
					   PANEL_MESSAGES,
					   sNOM_LIBRAIRIE,
					   0);
	ProcessSystemEvents();
}

// DEBUT ALGO
//******************************************************************************
// void QuitterAppli(void)
//******************************************************************************
//	- Paramètres CVI
//
//  - Quitte l'application
//
//  - 0 si OK, sinon code d'erreur
//******************************************************************************
// FIN ALGO
void QuitterAppli(void)
{
	int iYes;

	iYes = ConfirmPopup ("Warning", sMSG_WARNING_QUIT);
	if(iYes == 1)
	{
		//Modif MAximePAGES 09/09/2020********
		CDotNetHandle exception ;
		WuAutoCheck_WU_Automation_Test_and_Folder_Manager_CloseInterface(instanceLseNet,&exception);
		CDotNetDiscardHandle (instanceLseNet);
		Close_WuAutoCheck();
		//*********
		
		GbQuitterMain = 1;



		// Fermeture du port de communication
		if (GbVariatOpenComOk == 1)
		{
			VariateurCloseCom();
			GbVariatOpenComOk = 0;
		}

		// Fermeture du port de communication USB6008
		if (GbUsb6008OpenComOk == 1)
		{
			Usb6008Close();
			GbUsb6008OpenComOk = 0;
		}

		//------ Modif du 30/11/2010 ---- DEBUT --------
		if(GstConfig.iUseWatchdog == 1)
		{
			CmtWaitForThreadPoolFunctionCompletion (GiThreadPoolHandle1, GiThreadFctId1, OPT_TP_PROCESS_EVENTS_WHILE_WAITING);
			CmtReleaseThreadPoolFunctionID (GiThreadPoolHandle1, GiThreadFctId1);
			CmtDiscardThreadPool (GiThreadPoolHandle1);
		}
		//------ Modif du 30/11/2010 ---- FIN --------

		//Sleep(1000);  // we wait 500ms to acquire the information from the file
		DiscardPanel(GiPanelMain);

#ifdef bSIMULATION
		// Ecran de simulation
		DiscardPanelSimul();
#endif

		// Thread safe Var
		ModeUninitSafeVar();

		// Quitte l'application
		QuitUserInterface(0);
		//Sleep(1000);  // we wait 500ms to acquire the information from the file
	}
}

// DEBUT ALGO
//******************************************************************************
//int CVICALLBACK QuitterAccueil (int panel, int control, int event,
//		void *callbackData, int eventData1, int eventData2)
//******************************************************************************
//	- Paramètres CVI
//
//  - Quitte l'application
//
//  - 0 si OK, sinon code d'erreur
//******************************************************************************
// FIN ALGO
void CVICALLBACK QuitterAccueil(int menubar, int menuItem, void *callbackData, int panel)
{
	//Modif MaximePAGES  
	/*
	CDotNetHandle exception ;
	WuAutoCheck_WU_Automation_Test_and_Folder_Manager_CloseInterface(instanceLseNet,&exception);
	CDotNetDiscardHandle (instanceLseNet);
	Close_WuAutoCheck();
	FreeLibrary(GhDLL);
	*/

	QuitterAppli();
}



// DEBUT ALGO
//******************************************************************************
// void AffichageEcranAccueil(void)
//******************************************************************************
//	- Aucun
//
//  - Affichage de l'écran d'accueil
//
//  - Aucun
//******************************************************************************
// FIN ALGO
void AffichageEcranAccueil(void)
{
	DisplayPanel(GiPanelMain);
}

// DEBUT ALGO
//******************************************************************************
//void CVICALLBACK ModeFonctionnement (int menuBar, int menuItem, void *callbackData,
//		int panel)
//******************************************************************************
//	- paramètres CVI
//
//  - Affichage de l'écran correspondant au mode de fonctionnement choisi par l'opérateur
//
//  - 0 si Ok
//	  -1 sinon
//******************************************************************************
// FIN ALGO
void CVICALLBACK ModeFonctionnement (int menuBar, int menuItem, void *callbackData,
									 int panel)
{

	CDotNetHandle exception ;
	instanceLseNetModes=  instanceLseNet;

	switch(menuItem)
	{
			//case MENU_MODE_MODE_MANUEL:	   //Modif MaximePAGES 08/09/2020
			// Cache l'écran d'accueil
			HidePanel(GiPanelMain);

			// Affiche l'écran du mode manuel
			ModesDisplayPanel(iMODE_MANUEL);
			break;

			//case MENU_MODE_MODE_AUTOMATIQUE:		//Modif MaximePAGES 08/09/2020
			// Cache l'écran d'accueil
			HidePanel(GiPanelMain);

			// Affiche l'écran du mode automatique
			ModesDisplayPanel(iMODE_AUTOMATIQUE);
			break;

		case MENU_MODE_MODE_RUN:
			// Cache l'écran d'accueil
			HidePanel(GiPanelMain);
			// Affiche l'écran du mode automatique
			ModesDisplayPanel(iMODE_RUN);

			char ProjectDirectory[MAX_PATHNAME_LEN];
			char *confpath;
			if(GetProjectDir(ProjectDirectory) == 0)
			{
				confpath = strcat(ProjectDirectory,"\\Config\\DefaultConfig.txt");
				int fSize = 0;
				if(FileExists(confpath,fSize))
				{
					LoadConfiguration(ProjectDirectory);
				}
			}
			else
			{
				MessagePopup ("Error", "Could not load default paths configuration!");
			}
			break;
	}


}

// DEBUT ALGO
//******************************************************************************
// int TraductIhmAccueil(void)
//******************************************************************************
//	- Aucun
//
//  - Traduction de l'IHM
//
//  - 0 si Ok
//	  -1 sinon
//******************************************************************************
// FIN ALGO
int TraductIhmAccueil(void)
{
	int iErr;

	// Traduction des menus
	iErr = SetMenuBarAttribute (GetPanelMenuBar (GiPanelMain), MENU_MODE, 					ATTR_MENU_NAME, sOP_MODE_ACCUEIL);
	//iErr = SetMenuBarAttribute (GetPanelMenuBar (GiPanelMain), MENU_MODE_MODE_AUTOMATIQUE, 	ATTR_ITEM_NAME, sAUTO_MODE_ACCUEIL);
	//iErr = SetMenuBarAttribute (GetPanelMenuBar (GiPanelMain), MENU_MODE_MODE_MANUEL, 		ATTR_ITEM_NAME, sMANU_MODE_ACCUEIL);
	//iErr = SetMenuBarAttribute (GetPanelMenuBar (GiPanelMain), MENU_QUITTER, 				ATTR_MENU_NAME, sQUIT_ACCUEIL); //Modif MaximePAGES 08/09 
	//iErr = SetMenuBarAttribute (GetPanelMenuBar (GiPanelMain), MENU_A_PROPOS, 				ATTR_MENU_NAME, sABOUT_ACCUEIL);

	//disable mode automatique and manuel
	//SetMenuBarAttribute (GetPanelMenuBar (GiPanelMain), MENU_MODE_MODE_AUTOMATIQUE, ATTR_DIMMED, 1);  //Modif MaximePAGES 08/09/2020
	//SetMenuBarAttribute (GetPanelMenuBar (GiPanelMain), MENU_MODE_MODE_MANUEL, 	ATTR_DIMMED, 1);	 //Modif MaximePAGES 08/09/2020

	// Mise à jour du titre de l'écran d'accueil
	iErr = SetPanelAttribute (GiPanelMain, ATTR_TITLE, sTITRE_APPLICATION);

	// Traduction du titre au centre de l'écran
	//iErr = SetCtrlVal (GiPanelMain, PANEL_TITLE, sAPPLI_TITLE_ACCUEIL);

	// Traduction du bouton de réinitialisation
	iErr = SetCtrlAttribute (GiPanelMain, PANEL_BOUTON_REINIT, ATTR_LABEL_TEXT, sRESET_BUTT_ACCUEIL);

	// Traduction du titre de la zone de message
	iErr = SetCtrlAttribute (GiPanelMain, PANEL_MESSAGES, ATTR_LABEL_TEXT, sMESSAGES_ACCUEIL);

	return 0;
}


// DEBUT ALGO
//******************************************************************************
// int InitInstruments(stConfig *ptstConfig)
//******************************************************************************
//	- stConfig *ptstConfig : Pointeur sur les paramètres de configuration
//
//  - Initialisation des instruments
//
//  - 0 si Ok
//	  -1 sinon
//******************************************************************************
// FIN ALGO
int InitInstruments(stConfig *ptstConfig)
{
	char sMsg[500];
	int iErrVaria;
	int iErrUsb6008;
	int iEtatPorte1Ouverte;
	int iEtatPorte2Ouverte;
	int iEtatCapotSecuriteOuvert;
	int iEtatBoucleSecuriteOuverte;
	int iEtatMoteurArrete;
	int iEtatArretUrgenceActif;
	int iEtatCaptVibActif;
	int iNbEssais;
	double dPression;
	int iPortCom		= iCOM_VARIAT;
	long lVitesse		= iVIT_COM_VARIAT;
	float fVitesse;
	BOOL bError = 0;


	// Désactivation du menu
	SetMenuBarAttribute (GetPanelMenuBar (GiPanelMain), MENU_MODE, ATTR_DIMMED, 1);
	//SetMenuBarAttribute (GetPanelMenuBar (GiPanelMain), MENU_QUITTER, ATTR_DIMMED, 1);   //Modif MaximePAGES 08/09 
	//SetMenuBarAttribute (GetPanelMenuBar (GiPanelMain), MENU_A_PROPOS, ATTR_DIMMED, 1);
	// On désactive le bouton de réinitialisation
	SetCtrlAttribute(GiPanelMain, PANEL_BOUTON_REINIT, ATTR_DIMMED, 1);

	// Désactivation du menu
	SetMenuBarAttribute (GetPanelMenuBar (GiPanelMain), MENU_MODE, 		ATTR_DIMMED, 1);
	//SetMenuBarAttribute (GetPanelMenuBar (GiPanelMain), MENU_QUITTER, 	ATTR_DIMMED, 1);   //Modif MaximePAGES 08/09 
	//SetMenuBarAttribute (GetPanelMenuBar (GiPanelMain), MENU_A_PROPOS, 	ATTR_DIMMED, 1);

	// Désactivation du bouton de reset hardware
	SetCtrlAttribute (GiPanelMain, PANEL_BOUTON_REINIT, ATTR_DIMMED, 1);


	// Message d'initialisation
	AddMessageZM( "----------------------\n");
	AddMessageZM( sHARDWARE_INIT);
	bError = 0;

#ifdef bSIMULATION
	ProcessSystemEvents();
	Delay(0.5);
	ProcessSystemEvents();
#endif

	//------- Initialisation du module USB --------
	// Fermeture du port de communication si déjà ouvert
	if (GbUsb6008OpenComOk == 1)
	{
		Usb6008Close();
		GbUsb6008OpenComOk = 0;
	}

	iErrUsb6008 = Usb6008Init ();
	if (iErrUsb6008 != 0)
	{
		AddMessageZM(sERR_INIT_USB6008);
		bError = 1;
		GbUsb6008OpenComOk = 0;
	}
	else
	{
		AddMessageZM( sINIT_USB6008_OK);
		GbUsb6008OpenComOk = 1;
	}


	//------- Initialisation du variateur de vitesse -------
	//  - Initialisation de la communication avec le variateur
	iPortCom = ptstConfig->iSpeedComPort;
	lVitesse = ptstConfig->iSpeedComSpeed;

	// Fermeture du port de communication si déjà ouvert
	if (GbVariatOpenComOk == 1)
	{
		VariateurCloseCom();
		GbVariatOpenComOk = 0;
	}


	iErrVaria = VariateurInitCom (iPortCom, lVitesse);
	if (iErrVaria != 0)
	{
		AddMessageZM(sERR_INIT_ENGINE);
		bError = 1;
		GbVariatOpenComOk = 0;
	}
	else
	{
		AddMessageZM(sINIT_ENGINE_OK);
		GbVariatOpenComOk = 1;
	}

	//  - Commande de la ventilation du moteur
	iErrUsb6008 = Usb6008CommandeVentilationMoteur (iUSB6008_CMDE_VENTILATION_MOTEUR_INACTIFS);
	if (iErrUsb6008 != 0)
	{
		AddMessageZM(sERR_CTRL_ENGINE_FAN);
		bError = 1;
	}
	else
		AddMessageZM(sSTOP_ENGINE_FAN_OK);

	//  - Commande de l'ouverture de la gâche
	iErrUsb6008 = Usb6008CommandeOuvertureGache (iUSB6008_CMDE_OUVERTURE_GACHE_INACTIVE);
	if (iErrUsb6008 != 0)
	{
		AddMessageZM(sERR_CTRL_ELEC_LOCK);
		bError = 1;
	}
	else
		AddMessageZM(sCMD_ELEC_LOCK_OK);

	if (iErrUsb6008 == 0)
	{
		//  - Acquisition état des entrées logiques
		iErrUsb6008 = Usb6008AcquisitionEtats (&iEtatPorte1Ouverte, &iEtatPorte2Ouverte, &iEtatCapotSecuriteOuvert,
											   &iEtatBoucleSecuriteOuverte, &iEtatMoteurArrete, &iEtatArretUrgenceActif,
											   &iEtatCaptVibActif);

		if (iErrUsb6008 == 0)
		{
			if(iEtatPorte1Ouverte == iUSB6008_ETAT_PORTE_1_OUVERTE)
				AddMessageZM( sDOOR_1_OPENED);
			else
				AddMessageZM( sDOOR_1_CLOSED);

			if(iEtatPorte2Ouverte == iUSB6008_ETAT_PORTE_2_OUVERTE)
				AddMessageZM(sDOOR_2_OPENED);
			else
				AddMessageZM( sDOOR_2_CLOSED);

			if(iEtatCapotSecuriteOuvert == iUSB6008_ETAT_CAPOT_SECURITE_OUVERT)
				AddMessageZM(sSAFETY_HOOD_OPENED);
			else
				AddMessageZM(sSAFETY_HOOD_CLOSED);

			if(iEtatBoucleSecuriteOuverte == iUSB6008_ETAT_BOUCLE_SECURITE_OUVERTE)
				AddMessageZM( sSECUR_LOOP_OPENED);
			else
				AddMessageZM(sSECUR_LOOP_CLOSED);

			if(iEtatArretUrgenceActif == iUSB6008_ETAT_ARRET_URGENCE_ACTIF)
				AddMessageZM(sEMERGENCY_STOP_ACTIV);
			else
				AddMessageZM(sEMERG_STOP_INACTIV);

			if(iEtatCaptVibActif == iUSB6008_ETAT_VIBRATIONS_DETECT_ACTIF)
				AddMessageZM(sCAPT_VIB_ACTIV);
			else
				AddMessageZM(sCAPT_VIB_INACTIV);
		}
		else
		{
			AddMessageZM( sERR_READING_SECUR_STAT);
			bError = 1;
		}
	}


	//------- Mesure de la vitesse -----------
	if (iErrVaria == 0)
	{
		// Mesure vitesse courante
		iErrVaria = VariateurMesureVitesse (&fVitesse);
		if (fVitesse > fMIN_VITESSE_INIT)
		{
			// Affichage d'un message à l'écran
			AddMessageZM(sERR_TRY_TO_STOP_ENGINE);

			// Programmation de la rampe max
			iErrVaria = VariateurProgrammationRampe (fVIT_DEB_ARRET, fVIT_FIN_ARRET, dDUREE_ARRET);
			// Arrêt du moteur
			iErrVaria = VariateurArret();

			iNbEssais = 0;
			do
			{
				// Mesure de la vitesse courante
				iErrVaria = VariateurMesureVitesse (&fVitesse);
				// Affichage de la vitesse mesurée
				sprintf(sMsg, "Speed measure N°%d: %.03f trs/min", iNbEssais, fVitesse);
				AddMessageZM(sMsg);

				Delay(0.5);
				ProcessSystemEvents();
				iNbEssais ++;
			}
			while ((fVitesse > fMIN_VITESSE_INIT) && (iNbEssais < iNB_ESSAIS_MAX)&&(!GbQuitterMain));

			// Si la vitesse est encore trop élevée, affichage d'un message d'erreur
			if (fVitesse > fMIN_VITESSE_INIT)
			{
				sprintf(sMsg, sERR_SPEED_TOO_HIGH, fVitesse);
				// Affichage d'un message à l'écran
				AddMessageZM(sMsg);
			}
		}
		//--------------------------------------------------------
	}

	//------- Mesure de la pression -----------
	if (iErrUsb6008 == 0)
	{
		iErrUsb6008 = Usb6008MesurePression (&dPression);
		if(dPression > dMIN_PRESSION_SECURITE)
		{
			// Affichage d'un message à l'écran
			AddMessageZM(sRESET_PRESS);

			//  - Commande de la pression à zéro bars
			dPression = 0.0;
			iErrUsb6008 = Usb6008CommandePression (dPression);

			iNbEssais = 0;
			do
			{
				iErrUsb6008 = Usb6008MesurePression (&dPression);
				// Affichage de la pression mesurée
				sprintf(sMsg, "Pressure measure N°%d: %.03f bars", iNbEssais, (float)dPression);
				AddMessageZM(sMsg);

				Delay(0.5);
				ProcessSystemEvents();
				iNbEssais ++;
			}
			while ((dPression > dMIN_PRESSION_SECURITE)&&(iNbEssais < iNB_ESSAIS_MAX)&&(!GbQuitterMain));
		}

		iErrUsb6008 = Usb6008MesurePression (&dPression);
		if (iErrUsb6008 == 0)
		{
			if (dPression > dMIN_PRESSION_SECURITE)
			{
				sprintf(sMsg, sERR_PRESS_TOO_HIGH, (float)dPression);
				AddMessageZM(sMsg);
			}
		}
		else
			AddMessageZM(sERR_READ_PRESS);
	}
	//------------------------------------------

	if (iErrUsb6008 != 0)
		AddMessageZM(sERR_USB6008_COMM);

	if (iErrVaria != 0)
		AddMessageZM(sERR_VARIATOR);


	// Message de fin de l'initialisation hardware
	AddMessageZM(sEND_HARD_INIT);

	// Réactivation du menu
	SetMenuBarAttribute (GetPanelMenuBar (GiPanelMain), MENU_MODE, 		ATTR_DIMMED, 0);
	//SetMenuBarAttribute (GetPanelMenuBar (GiPanelMain), MENU_QUITTER, 	ATTR_DIMMED, 0);   //Modif MaximePAGES 08/09
	//SetMenuBarAttribute (GetPanelMenuBar (GiPanelMain), MENU_A_PROPOS, 	ATTR_DIMMED, 0);

	// Réactivation du bouton de reset hardware
	SetCtrlAttribute (GiPanelMain, PANEL_BOUTON_REINIT, ATTR_DIMMED, 0);

	// Activation du menu
	SetMenuBarAttribute (GetPanelMenuBar (GiPanelMain), MENU_MODE, 		ATTR_DIMMED, 0);
	//SetMenuBarAttribute (GetPanelMenuBar (GiPanelMain), MENU_QUITTER, 	ATTR_DIMMED, 0);	//Modif MaximePAGES 08/09 
	//SetMenuBarAttribute (GetPanelMenuBar (GiPanelMain), MENU_A_PROPOS, 	ATTR_DIMMED, 0);

	// On active le bouton de réinitialisation
	SetCtrlAttribute(GiPanelMain, PANEL_BOUTON_REINIT, ATTR_DIMMED, 0);


	if ((iErrUsb6008 != 0) || (iErrVaria != 0))
		return -1;


	return 0;
}


// DEBUT ALGO
//******************************************************************************
//int CVICALLBACK Reinitialisation (int panel, int control, int event,
//		void *callbackData, int eventData1, int eventData2)
//******************************************************************************
//	- Paramètres CVI
//
//  - Réinitialisation des instruments
//
//  - 0 si OK, sinon code d'erreur
//******************************************************************************
// FIN ALGO
int CVICALLBACK Reinitialisation (int panel, int control, int event,
								  void *callbackData, int eventData1, int eventData2)
{
	int iErr;

	switch (event)
	{
		case EVENT_COMMIT:
			iErr = InitInstruments(&GstConfig);
			break;
	}

	return 0;
}

// DEBUT ALGO
//******************************************************************************
//int CVICALLBACK CloseAbout (int panel, int event, void *callbackData,
//								int eventData1, int eventData2)
//******************************************************************************
//	- paramètres CVI
//
//  - Fermeture de l'écran "About"
//
//  - 0 si Ok
//	  -1 sinon
//******************************************************************************
// FIN ALGO

int CVICALLBACK CloseAbout (int panel, int control, int event,
							void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{
		case EVENT_COMMIT:
			RemovePopup(GiPanelAbout);
			break;
	}
	return 0;
}

// DEBUT ALGO
//******************************************************************************
//void CVICALLBACK LoadAbout (int menuBar, int menuItem, void *callbackData,
//								int panel)
//******************************************************************************
//	- paramètres CVI
//
//  - Affichage de l'écran "About"
//
//  - 0 si Ok
//	  -1 sinon
//******************************************************************************
// FIN ALGO
void CVICALLBACK LoadAbout (int menuBar, int menuItem, void *callbackData,
							int panel)
{
	// Affichage de l'écran "About"
	InstallPopup(GiPanelAbout);
}

// DEBUT ALGO
//******************************************************************************
//int CVICALLBACK BtnQuitter (int panel, int control, int event,
//		void *callbackData, int eventData1, int eventData2)
//******************************************************************************
//	- paramètres CVI
//
//  - Bouton quitter
//
//  - 0 si Ok
//	  -1 sinon
//******************************************************************************
// FIN ALGO
int CVICALLBACK BtnQuitter (int panel, int control, int event,
							void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{
		case EVENT_COMMIT:
			QuitterAppli();
			break;
	}
	return 0;
}

//------ Modif du 30/11/2010 ---- DEBUT --------
// DEBUT ALGO
//******************************************************************************
// static int CVICALLBACK ThreadWatchdog (void *functionData)
//******************************************************************************
//	- Paramètres CVI
//
//  - Création du thread de gestion du watchdog
//
//  - Code de retour (0 = OK)
//******************************************************************************
// FIN ALGO
static int CVICALLBACK ThreadWatchdog (void *functionData)
{
	char sFileName[MAX_FILENAME_LEN];
	char sNewFileName[MAX_FILENAME_LEN];
	int iVitesse;
	int iPression;
	int iDurEssaiVit;
	int iDurEssaiPress;
	int iLastDurEssaiVit=0;
	int iLastDurEssaiPress=0;
	int bRename=0;

	strcpy(sFileName, 		"");
	strcpy(sNewFileName, 	"");

	do
	{
		if(GiCpt < 20)
			GiCpt ++;
		else
			GiCpt = 0;

		GetFirstFile ("watch_*.txt", 1, 0, 0, 0, 0, 0, sFileName);

		ModesGetEtatCycles(&iVitesse, &iPression, &iDurEssaiVit, &iDurEssaiPress);
		if(GbQuitterMain)
			strcpy(sNewFileName, "watch_quitter.txt");
		else
		{
			// Il y a au moins un cycle en cours
			if ((iVitesse != 0) || (iPression != 0))
			{
				bRename = 1;

				// Cycle de vitesse
				if (iVitesse != 0)
				{
					if (iLastDurEssaiVit != iDurEssaiVit)
						iLastDurEssaiVit =  iDurEssaiVit;
					else
						bRename = 0;
				}

				// Cycle de pression
				if (iPression != 0)
				{
					if (iLastDurEssaiPress != iDurEssaiPress)
						iLastDurEssaiPress	= iDurEssaiPress;
					else
						bRename = 0;
				}

				if (bRename)
					sprintf(sNewFileName, "watch_%03d.txt", GiCpt);
			}
			else
				strcpy(sNewFileName, "watch_standby.txt");
		}

		RenameFile (sFileName,sNewFileName);


		if(!GbQuitterMain);
		Delay(2.0);
		//ProcessSystemEvents();
	}
	while (!GbQuitterMain);

	strcpy(sNewFileName, "watch_quitter.txt");
	RenameFile (sFileName,sNewFileName);

	return 0;
}


// DEBUT ALGO
//******************************************************************************
// void CreateThreadWatchdog(void)
//******************************************************************************
//	- Aucun
//
//  - Création du thread de gestion du watchdog
//
//  - Aucun
//******************************************************************************
// FIN ALGO
void CreateThreadWatchdog(void)
{
	int iStatus;

	// Initialisation du thread
	iStatus = CmtNewThreadPool (UNLIMITED_THREAD_POOL_THREADS, &GiThreadPoolHandle1);
	iStatus = CmtScheduleThreadPoolFunction (GiThreadPoolHandle1, ThreadWatchdog, NULL, &GiThreadFctId1);
}
//------ Modif du 30/11/2010 ---- FIN --------

// DEBUT ALGO
//******************************************************************************
//int __stdcall WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance,
//					   LPSTR lpszCmdLine, int nCmdShow)
//******************************************************************************
//	- paramètres CVI
//
//  - Programme principal
//
//  - 0 si Ok
//	  -1 sinon
//******************************************************************************
// FIN ALGO
/*	 void display()
	 {   
		 int i=0; 
		 while (strcmp(testtab[i],"")!=0)
		 {
			 printf("testtab[%d] = _%s_\n",i,testtab[i]);
			 i++;
		 }
		 
	 }
*/
int __stdcall WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance,
					   LPSTR lpszCmdLine, int nCmdShow)
{
	
	
	FILE *hFile;
	HANDLE hProcess;
	DWORD wPriority;
	DWORD dwEnCours;
	int dwStatus;
	int iErr;
	char sMsgErr[1000];
	char sCheminEcran[MAX_PATHNAME_LEN];
	char sCheminTraduc[MAX_PATHNAME_LEN];
	char sCheminConfig[MAX_PATHNAME_LEN];

	unsigned int NbByte;
	unsigned int NbBlock;

	char sAnumPath[MAX_PATHNAME_LEN];
	
	  int iRet=0;
    unsigned char cAPI;
    char aCommand[128];
    int iZ;
    int iN;
	//**************MARVYN 12/08/2021*******************
    unsigned int threadStyle = COINIT_APARTMENTTHREADED;
	//Valid values are COINIT_MULTITHREADED and COINIT_APARTMENTTHREADED.
	CA_InitActiveXThreadStyleForCurrentThread (0, threadStyle);
	//**************************************************
	
	GbQuitterMain = 0;

	// Interdiction de relancer l'application si une instance existe
	CheckForDuplicateAppInstance (ACTIVATE_OTHER_INSTANCE, &dwStatus);
	if (dwStatus)
		return 0;

	if (InitCVIRTE (hInstance, 0, 0) == 0)
		return -1;    // out of memory 

	// Récupération répertoire de l'application
	GetProjectDir (GsPathApplication);

	sprintf (sAnumPath, "%s\\%s", GsPathApplication, "IPC.dll");

	
	  // Load Library
       // printf("\n* Loading DLL: %s\n", sAnumPath);
        hDLL=LoadLibrary(sAnumPath);
        if (hDLL==NULL)
        {
            //printf("   Error : Loading DLL\n");
        }

        // Get function
        API=(TAPI)GetProcAddress(hDLL, "API");
        if (API==NULL)
        {
            //printf("Error : API function not found\n");
        }
        //printf ("   OK\n");


        // Necessary to ANumLFRF to be executed in background
        Sleep(2000);

	
	
	GhDLL=NULL;
	GapiANum=0;
	//MessagePopup("info","Load anum dll!");
	GhDLL=LoadLibrary(sAnumPath);
	//hDLL=LoadLibrary("API.dll");
	if (GhDLL!=NULL)
	{
		// Get function
		GapiANum=(TAPI)GetProcAddress(GhDLL, "API");
		//MessagePopup("info","Load api dll!");
	}
	
	Sleep(2000); 

	//---- Modification du 04/04/2013 par CB --- DEBUT ------
	ZoneMessageLoad(sSOUS_REP_TRACE);
	//---- Modification du 04/04/2013 par CB --- FIN ------

	// Formation du chemin complet vers l'écran d'accueil
	sprintf (sCheminEcran, "%s\\%s\\%s", GsPathApplication, sREP_ECRANC, sNOM_IHM_PRINCIPALE);
	// Chargement de l'IHM d'acccueil
	if ((GiPanelMain = LoadPanel (0, sCheminEcran, PANEL)) < 0)
	{
		MessagePopup (sTITRE_APPLICATION, "Don't find screen ["sNOM_IHM_PRINCIPALE"]!");
		return -1;
	}


	// Initialisation des variables protégées
	ModeInitSafeVar();


	// Fixe le chemin vers le fichier de traduction
	sprintf(sCheminTraduc, "%s\\%s\\%s", GsPathApplication, sREP_CONFIG, sTRADUC_FILE);
	//MessageIhmSetPathFile(sCheminTraduc);

	// Traduction de l'IHM principale
	iErr = TraductIhmAccueil();

	// Formation du chemin complet vers le fichier de configuration
	sprintf (sCheminConfig, "%s\\%s\\%s", GsPathApplication, sREP_CONFIG, sCONFIG_FILE);
	// Lecture du fichier de configuration
	iErr = GstFilesReadConfigFile(sCheminConfig, &GstConfig, sMsgErr);
	if (iErr != 0)
	{
		MessagePopup (sTITRE_APPLICATION, sMsgErr);
		return -1;
	}

	// Chargement de l'écran des modes automatique/manuel
	iErr = ModesLoadPanel(&AffichageEcranAccueil, sREP_ECRANC, 0, &GstConfig, GstConfig.iSpeedUnit, GstConfig.iPressUnit);

	SetCtrlAttribute(GiPanelMain, PANEL_BOUTON_REINIT, ATTR_DIMMED, 1);

	// Affichage de l'écran d'accueil
	DisplayPanel(GiPanelMain);

#ifdef bSIMULATION
	// Affichage de l'écran de simulation
	LoadPanelSimul();
#endif

	// Initialisations du variateur de vitesse et du régulateur de pression
	InitInstruments(&GstConfig);

	SetCtrlAttribute(GiPanelMain, PANEL_BOUTON_REINIT, ATTR_DIMMED, 0);
	//init LseNEt
	//Initialize_CheckLse();
	//MessagePopup("info","Init Check dll!");
	Initialize_WuAutoCheck();


	CDotNetHandle exception ;
	//MessagePopup("info","Load check dll!");
	WuAutoCheck_WU_Automation_Test_and_Folder_Manager__Create(&instanceLseNet,&exception);
	//MessagePopup("info","OK!");
	//---- Modif du 30/11/2010 par C.BERENGER --- DEBUT ----------
	//--------------------------------------------------------------------------
	// Lancment de la tâche de modification du nom de fichier (pour le watchdog
	DeleteFile ("watch_*.*");

	// Si l'on doit utiliser le watchdog
	if(GstConfig.iUseWatchdog == 1)
	{
		hFile = fopen ("watch_standby.txt", "w");
		fclose(hFile);

		// Création des threads
		CreateThreadWatchdog();

		// Lancement du programme de watchdog
		//LaunchExecutableEx ("watchdog.exe", LE_HIDE, NULL);
		LaunchExecutableEx ("watchdog.exe", LE_HIDE, NULL);
	}
	//--------------------------------------------------------------------------
	//---- Modif du 30/11/2010 par C.BERENGER --- FIN ----------





	// Attente actions utilisateur
	//SetSleepPolicy (VAL_SLEEP_NONE);
	RunUserInterface();
	

	return 0;
}
			  

			  
