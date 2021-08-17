// DEBUT TABLE
//******************************************************************************
// PRO: BANC DE TEST LSE
// PRG: Modes.h
// VER: Version I00A00A du 09/08/2010
// AUT: C. BERENGER / MICROTEC
//
// ROL: Gestion de l'écran de fonctionnement en mode Automatique/Manuel
//
// HST: Version I00A00A du 09/08/2010 / C. BERENGER MICROTEC / Création
//******************************************************************************
//
//*************************************************************************** 
// FIN TABLE

#ifndef LIB_MODES
 #define LIB_MODES

//***************************************************************************
// inclusions
//***************************************************************************

// Librairies standards

// Librairie FOURNISSEUR

// Librairie MICROTEC

// Librairie spécifiques à l'application


//***************************************************************************
// Définition de constantes
//***************************************************************************
#define iMODE_MANUEL 		0
#define iMODE_AUTOMATIQUE 	1
#define iMODE_RUN 			2 

#define sTITRE_APPLICATION	"ACASYA"

#define iUNIT_KM_H		0
#define iUNIT_G			1
#define iUNIT_TRS_MIN	2

#define iUNIT_KPA		0
#define iUNIT_BAR		1
#define iUNIT_MBAR		2
#define iUNIT_PSI		3

//--------------------------------------
#define iETAT_ATTENTE_AFFICH_IHM		0
#define iETAT_ATTENTE					1
#define iETAT_DEM_CLOSE_ELECTRIC_LOCK   2
#define iETAT_DEM_APP_BOUCLE_SECUR 		3
#define iETAT_ATT_DEP_ESSAI				4
#define iETAT_ESSAI_EN_COURS			5

#define sMSG_OK_SECUR_LOOP_CLOSED "OK, security loop closed!"
#define sMSG_WAIT_START_TEST	 "---- Waiting start test... -----"
#define sMSG_PUSH_SEC_LOOP_BUTT	 "Please, push the Security Loop Blue Button."
#define sMSG_SECURE_BENCH		 "Wait for secured bench (close doors, close safety hood, remove emergency stop)"
#define sMSG_TEST_RUNNING		 "Test running..."
                           
#define sMSG_CANT_START_SPEED_CYCLE "Can't start speed cycle (number of steps < 2)! "
#define sMSG_CANT_START_PRESS_CYCLE "Can't start pressure cycle (number of steps < 2)!" 
#define sMSG_ERR_ADD_ROWS		 "Error adding rows"
#define sMSG_ERR_DEL_ROWS		 "Error deleting rows"
#define sMSG_ERR_COPY_DATA		 "Error copying data"
#define sMSG_ERR_PASTE_DATA		 "Error pasting data"
#define sMSG_ERR_NO_DATA_AVAIL	 "No data available"
#define sMSG_ERR_MAJ_DATA		 "Data Maj error"
#define sMSG_ERR_INSERT			 "Insert Error"
#define sMSG_ERR_LOADING_FILE	 "Error loading File"
#define sMSG_ERR_READING_FILE	 "Error reading File"
#define sMSG_ERR_SAV_CYCLES		 "Error saving cycles"
#define sMSG_SAVE_DATA_ON_FILE	 "You must save data on file before starting test!"
#define sMSG_ERR_CREAT_RES_DIR	 "Error creating result directory!"
#define sMSG_ENT_MEAS_SPEED		 "Speed Measure N\°;Time elapsed(s); Speed Cycle Index;Speed Step Index;Speed Setpoint;"
#define sMSG_ENT_MEAS_PRESS		 ";<-->;Press Measure N\°;Time elapsed(s);Press Cycle Index;Press Step Index;Pressure Setpoint;"
#define sMSG_ERR_ADD_RES_LINE	 "Error adding result line"
#define sMSG_ERR_SECU_DFLT_DETEC "Security default detected!"
#define sMSG_CLOSE_ELECTRIC_LOCK "Close Electric Lock (see IHM Button)"

#define iMSG_OK_SECUR_LOOP_CLOSED	44
#define iMSG_WAIT_START_TEST		45
#define iMSG_PUSH_SEC_LOOP_BUTT		46
#define iMSG_SECURE_BENCH			47
#define iMSG_TEST_RUNNING			48

#define iMSG_CANT_START_SPEED_CYCLE 49
#define iMSG_CANT_START_PRESS_CYCLE 50
#define iMSG_ERR_ADD_ROWS			51
#define iMSG_ERR_DEL_ROWS			52
#define iMSG_ERR_COPY_DATA			53
#define iMSG_ERR_PASTE_DATA			54
#define iMSG_ERR_NO_DATA_AVAIL		55
#define iMSG_ERR_MAJ_DATA			56
#define iMSG_ERR_INSERT				57
#define iMSG_ERR_LOADING_FILE		58
#define iMSG_ERR_READING_FILE		59
#define iMSG_ERR_SAV_CYCLES			60
#define iMSG_SAVE_DATA_ON_FILE		61
#define iMSG_ERR_CREAT_RES_DIR		62
#define iMSG_ENT_MEAS_SPEED			63
#define iMSG_ENT_MEAS_PRESS			64
#define iMSG_ERR_ADD_RES_LINE		65
#define iMSG_ERR_SECU_DFLT_DETEC	66
#define iMSG_CLOSE_ELECTRIC_LOCK	67
//--------------------------------------

#define iMAX_CHAR_COMMENTS		    10000

//---- Type de table (vitesse ou pression)
#define iTYPE_VITESSE				1
#define iTYPE_PRESSION				2

#define fVIT_DEB_ARRET				1800.0
#define fVIT_FIN_ARRET				0.0
#define dDUREE_ARRET				3.0

#define iLEVEL_WARNING				1
#define iLEVEL_MSG					2

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
#define sREP_RESSULTATS				"Results"

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

//******************************************************************************
// Définition de types et structures
//******************************************************************************



//******************************************************************************
// Fonctions externes au source
//******************************************************************************

//***************************************************************************
// void ModeUninitSafeVar(void);
//***************************************************************************
//	- Aucun
//
//  - Libération de la mémoire occupée par les variables protégées
//
//  - Aucun
//***************************************************************************
// FIN ALGO
void ModeUninitSafeVar(void);

//***************************************************************************
// void ModeInitSafeVar(void);
//***************************************************************************
//	- Aucun
//
//  - Initialisation des variables protégées
//
//  - Aucun
//***************************************************************************
void ModeInitSafeVar(void);

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
void ModesGetEtatCycles(int *iVitesse, int *iPression, int *iDurEssVit, int *iDurEssPress);

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
int ModesLoadPanel(	void *FonctionRetourIhm,
					char *sSousRepertoireEcran,
					int iPanelPere,
					stConfig *ptStConfig,
					int iUnitVitesse,
					int iUnitPression);

//****************************************************************************** 
//	- int iMode : Mode automatique ou manuel
//
//  - Affichage de l'écran du mode automatique ou manuel
//
//  - 0 si OK, sinon code d'erreur
//******************************************************************************
void ModesDisplayPanel(int iMode);

//****************************************************************************** 
//	- int iPanel		: Handle du panel d'essai
//	  int iStatusPanel	: Handle du panel de status
//	  char *sTitle		: Titre 
//	  char *sMsg		: Message à afficher
//	  int *ptiPanelStatDisplayed	: 1=Ecran déjà affiché, sinon 0
//	  int *ptiAffMsg	: 1=Message affiché, sinon, 0
//
//  - Affichage du panel de status
//
//  - 0 si OK, sinon code d'erreur
//****************************************************************************** 
void AffStatusPanel(int iPanel, int iStatusPanel, int iLevel, char *sTitle, char *sMsg,
					int *ptiPanelStatDisplayed, int *ptiAffMsg);

void InsertTestCasesIntoTables(int RowPosition, int panel);
BOOL CheckPrecondition();
void SaveConfiguration(int iRet, char *sProjectDir);
void LoadConfiguration(char *sFileName);
void GetDataFromSelection(Rect rect, char PathsArray[500][500], char RNoArray[500][500], int *RowNb);
void InsertDataFromSelection(Point InsertCell, char PathsArray[500][500], char RNoArray[500][500], int RowNb);
void LoadANumConfiguration(char sFileName[MAX_PATHNAME_LEN]);
int DataCheck(const void *a, const void *b);
void add_menu();
void couleurLigne(int numeroLigne, int GiPanel);
void insert_steps(int numeroLigne, int GiPanel);

//****************************************************************************** 
//Modif MaximePAGES 02/06/2020	  au 11/09/2020

//****************************************************************************** 
//** Gestion des processus Excel (certains processus excel sont lancés durant 
// l'execution mais jamais manuellement arretés). 
//******************************************************************************  
typedef struct List List;  
typedef struct Element Element;   
void insert(List *list, int newNumber); 
List *initialisationList(); 
void insertElement(List *list, int newNumber);
void removeFirstElement(List *list);
void removeAllElements(List *list); 
void killAllProcExcel(List *listPIDexcel);    

int returnLastExcelPID();
void killPIDprocess(int pid);
//****************************************************************************** 



////////////////////////////////////////////////////////////////////////////////
//** Fichier texte de degug
////////////////////////////////////////////////////////////////////////////////   
void writeLogDebugFile(char * commentaire, float other, char * filesource, int line) ;
////////////////////////////////////////////////////////////////////////////////  


////////////////////////////////////////////////////////////////////////////////  
//** printCheckResults : fonction qui imprime dans le fichier CheckResults.txt 
// le statut FAILLED ou PASSED de chaque function check. 
////////////////////////////////////////////////////////////////////////////////  
void printCheckResults(FILE * file_handle, char * functionName , int valueReturned, nbOfValidData, nbOfInvalidData, nbOfFrames, nbOfBursts, userValue, double average, double max, double min);
////////////////////////////////////////////////////////////////////////////////  


////////////////////////////////////////////////////////////////////////////////  
//** CheckCompareP : fonction check qui compare les valeurs de pression 
// envoyées par le capteur avec celles du banc LSE 
int CheckCompareP(char * wuID,double dtolValue, double dtolpercent, char *Param, char* InterfacetoCheck);
//////////////////////////////////////////////////////////////////////////////// 


////////////////////////////////////////////////////////////////////////////////  
//** CheckCompareAcc : fonction check qui compare les valeurs d'acceleration
// envoyées par le capteur avec celles du banc LSE 
int CheckCompareAcc(char * wuID,char * value, double dtolValue, double dtolpercent, char *FCParam, char* InterfacetoCheck);
////////////////////////////////////////////////////////////////////////////////  


////////////////////////////////////////////////////////////////////////////////  
//** Fonctions pour le CheckSTDEV : vérifier l'intégrité de la LSE
int CheckPreSTDEV(char * wuID, char *Value, char *sFrameNb, char *sAngleEmission, char *realFCParam, char *FunctionCodeValue, char *Parametertocheck, char *InterfacetoCheck);
int CheckIndivSTDEV();
int CheckNewSTDEV(char *sAngleEmission,int errorPreSTDEV,int errorIndivSTDEV); 
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////  
//** generateCoverageMatrix : fonction qui génère le CoverageMatrix (ficher 
// excel synthetisant l'ensemble des tests d'une séquence
void generateCoverageMatrix(char * currentDirPath, int currentTestStatus);
////////////////////////////////////////////////////////////////////////////////   


////////////////////////////////////////////////////////////////////////////////  
//** fonctiond permettant de griser ou non le clavier numérique dans Script
// definition et Expect Results definition 
void dimmedNumericKeypadForScript(int state);
void dimmedNumericKeypadForExp(int state); 
////////////////////////////////////////////////////////////////////////////////  


////////////////////////////////////////////////////////////////////////////////  
//** fonctiond permettant de traduire une formule de paramètre en valeur (float ou string)
float parameterToValue (char * formula) ; 
char * parameterToValue_Str (char * formula) ;
////////////////////////////////////////////////////////////////////////////////    


////////////////////////////////////////////////////////////////////////////////  
//** translateScriptExcel : traduit l'excel Test Case avec paramètres en excel 
// Test Case avec des valeurs seulement
void translateScriptExcel (char * excelScript, char * newExcelScript);
//////////////////////////////////////////////////////////////////////////////// 


//////////////////////////////////////////////////////////////////////////////// 
//** generateParameterTab : crée un tableau de paramètres (ceux provenant de la Database)
void generateParameterTab (CAObjHandle worksheetHandle) ;
////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////// 
//** isNumber : renvoie 1 si le string donné en arg est un nombre 
int isNumber(char * mynum);
////////////////////////////////////////////////////////////////////////////////  

////////////////////////////////////////////////////////////////////////////////      
//** Renvoie un chiffre indiquant si le string donnée en arg est une valeur
// fixe, un range ou une séquence
int typeOfValue(char *str); 
//////////////////////////////////////////////////////////////////////////////// 



////////////////////////////////////////////////////////////////////////////////      
//** checkCurrentSeqSaved : permet de déterminer si la séquence a été bien enregistrée ou 
// non en comparant la séquence en cours (enregistrée temporairement dans un .txt
// avec la dernière sequence sauvergardé (dans un fichier .txt)
int checkCurrentSeqSaved (char *sProjectDir);
int compareFile(FILE * fPtr1, FILE * fPtr2, int * line, int * col);     
//////////////////////////////////////////////////////////////////////////////// 

struct Parameter
{
	char name[200];
	char value[100];
	char unit[150];
};

struct Frame
{
	char anglepos[5];
	char angleval[10];
};

//fonction C 
char * strtok_r (char *s, const char *delim, char **save_ptr);  

//MARVYN 23/06/2021
void couleurSelection(int numeroLigne,int control, int GiPanel, int color) ;

void modifyStep(int rowNb);

void MoveString(char* label);

int verifLabel(char *label);

//int replaceSpaces(char *string);
int PathSpaces(char *string);
char* configInit(char type);
char *configName(char* path);
int labelAlreadyInsert(char *label);
int labelAlreadyExists(char *label);
char* StandardToAttribute(char *Param);
char* DiagToAttribute(char *Param) ;
char* SW_IndentToAttribute(char *Param)  ;
char* DruckToAttribute(char *Param)  ; 
char* LSEToAttribute(char *Param)  ;
int GetALLFramesTypes();
int CheckRFProtocol(char *InterfaceToCheck,char *RFprot);
int findColumn(char *name);

struct interfaceStruct
{
 char columnParam[10] ;
 char columnAttribute[10] ; 
 char name[80] ;
};

struct column
{
 char columnParam[10] ;
 char columnAttribute[10] ; 
};

//******************************************************************************
#endif
