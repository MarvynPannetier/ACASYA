/**************************************************************************/
/* LabWindows/CVI User Interface Resource (UIR) Include File              */
/*                                                                        */
/* WARNING: Do not add to, delete from, or otherwise modify the contents  */
/*          of this include file.                                         */
/**************************************************************************/

#include <userint.h>

#ifdef __cplusplus
    extern "C" {
#endif

     /* Panels and Controls: */

#define  EXPRESULTS                       1
#define  EXPRESULTS_EXPLIST               2       /* control type: ring, callback function: expected_list */
#define  EXPRESULTS_TAKEFIELD             3       /* control type: string, callback function: (none) */
#define  EXPRESULTS_WUIDEXP               4       /* control type: ring, callback function: wuid_fonction_exp */
#define  EXPRESULTS_LABEL2                5       /* control type: string, callback function: (none) */
#define  EXPRESULTS_LABEL1                6       /* control type: string, callback function: (none) */
#define  EXPRESULTS_EXPVALUE_2            7       /* control type: numeric, callback function: (none) */
#define  EXPRESULTS_VALUE                 8       /* control type: string, callback function: exp_value */
#define  EXPRESULTS_SELECTPARAM           9       /* control type: ring, callback function: selectparam */
#define  EXPRESULTS_ADDEXP                10      /* control type: command, callback function: addexp */
#define  EXPRESULTS_OKBUTTON              11      /* control type: command, callback function: OkCallback */
#define  EXPRESULTS_EXPUNIT               12      /* control type: string, callback function: (none) */
#define  EXPRESULTS_DECORATION_7          13      /* control type: deco, callback function: (none) */
#define  EXPRESULTS_DECORATION_8          14      /* control type: deco, callback function: (none) */
#define  EXPRESULTS_WUID_VALUE            15      /* control type: string, callback function: (none) */
#define  EXPRESULTS_DECORATION_2          16      /* control type: deco, callback function: (none) */
#define  EXPRESULTS_EXPVALUE              17      /* control type: string, callback function: (none) */
#define  EXPRESULTS_FORMUEXPEC            18      /* control type: string, callback function: (none) */
#define  EXPRESULTS_BUTDOT                19      /* control type: command, callback function: butdot */
#define  EXPRESULTS_BUT0                  20      /* control type: command, callback function: but0 */
#define  EXPRESULTS_INSERTFORMULE         21      /* control type: command, callback function: insertformule */
#define  EXPRESULTS_BUTSUM                22      /* control type: command, callback function: butsum */
#define  EXPRESULTS_BUT3                  23      /* control type: command, callback function: but3 */
#define  EXPRESULTS_BUT2                  24      /* control type: command, callback function: but2 */
#define  EXPRESULTS_BUTSUB                25      /* control type: command, callback function: butsub */
#define  EXPRESULTS_BUTF                  26      /* control type: command, callback function: butF */
#define  EXPRESULTS_BUTE                  27      /* control type: command, callback function: butE */
#define  EXPRESULTS_BUT1                  28      /* control type: command, callback function: but1 */
#define  EXPRESULTS_BUT6                  29      /* control type: command, callback function: but6 */
#define  EXPRESULTS_BUTD                  30      /* control type: command, callback function: butD */
#define  EXPRESULTS_BUT5                  31      /* control type: command, callback function: but5 */
#define  EXPRESULTS_BUTC                  32      /* control type: command, callback function: butC */
#define  EXPRESULTS_BUTMULT               33      /* control type: command, callback function: butmult */
#define  EXPRESULTS_BUT4                  34      /* control type: command, callback function: but4 */
#define  EXPRESULTS_BUTB                  35      /* control type: command, callback function: butB */
#define  EXPRESULTS_DELETEBUTTON          36      /* control type: command, callback function: deletebutton */
#define  EXPRESULTS_BUTDIV                37      /* control type: command, callback function: butdiv */
#define  EXPRESULTS_BUTA                  38      /* control type: command, callback function: butA */
#define  EXPRESULTS_BUT9                  39      /* control type: command, callback function: but9 */
#define  EXPRESULTS_BUT8                  40      /* control type: command, callback function: but8 */
#define  EXPRESULTS_BUT7                  41      /* control type: command, callback function: but7 */
#define  EXPRESULTS_BUTPAR2               42      /* control type: command, callback function: butpar2 */
#define  EXPRESULTS_BUTPAR1               43      /* control type: command, callback function: butpar1 */
#define  EXPRESULTS_DECORATION_3          44      /* control type: deco, callback function: (none) */
#define  EXPRESULTS_DECORATION_5          45      /* control type: deco, callback function: (none) */
#define  EXPRESULTS_TOLERENCE2            46      /* control type: string, callback function: (none) */
#define  EXPRESULTS_TOLERENCE1            47      /* control type: string, callback function: (none) */
#define  EXPRESULTS_FIELDCHECK            48      /* control type: ring, callback function: fieldtocheck */
#define  EXPRESULTS_PARAMETERSDATA        49      /* control type: ring, callback function: parametersdata */
#define  EXPRESULTS_ATTRIBUTENAME         50      /* control type: string, callback function: (none) */
#define  EXPRESULTS_VALUEFC               51      /* control type: string, callback function: (none) */
#define  EXPRESULTS_DELTOLPER             52      /* control type: command, callback function: deltolper */
#define  EXPRESULTS_DELTOL                53      /* control type: command, callback function: deltol */
#define  EXPRESULTS_DELVALUE              54      /* control type: command, callback function: delvalue */
#define  EXPRESULTS_ADDVAL                55      /* control type: command, callback function: addval */
#define  EXPRESULTS_ADDTOL                56      /* control type: command, callback function: addtol */
#define  EXPRESULTS_ADDTOL_2              57      /* control type: command, callback function: addtol_2 */
#define  EXPRESULTS_TEXTMSG_RANGEVALUE    58      /* control type: textMsg, callback function: (none) */
#define  EXPRESULTS_TEXTMSG_SEQVALUE      59      /* control type: textMsg, callback function: (none) */
#define  EXPRESULTS_TEXTMSG_FIXEDVALUE    60      /* control type: textMsg, callback function: (none) */
#define  EXPRESULTS_TEXTMSG               61      /* control type: textMsg, callback function: (none) */
#define  EXPRESULTS_TXTUNITLABEL          62      /* control type: textMsg, callback function: (none) */
#define  EXPRESULTS_TXTUNITBODY           63      /* control type: textMsg, callback function: (none) */
#define  EXPRESULTS_BTNTOLPRCT            64      /* control type: radioButton, callback function: btntolprct */
#define  EXPRESULTS_BTNTOLVAL             65      /* control type: radioButton, callback function: btntolval */
#define  EXPRESULTS_TEXTINCH              66      /* control type: textMsg, callback function: (none) */
#define  EXPRESULTS_DECORATION_4          67      /* control type: deco, callback function: (none) */
#define  EXPRESULTS_WHEELDIM              68      /* control type: string, callback function: (none) */
#define  EXPRESULTS_TEXTMSG_8             69      /* control type: textMsg, callback function: (none) */
#define  EXPRESULTS_TEXTMSG_7             70      /* control type: textMsg, callback function: (none) */
#define  EXPRESULTS_DECORATION_9          71      /* control type: deco, callback function: (none) */
#define  EXPRESULTS_DECORATION_6          72      /* control type: deco, callback function: (none) */

#define  PANEL_ADD                        2
#define  PANEL_ADD_LFDNBFRAME             2       /* control type: ring, callback function: lfdnbframe */
#define  PANEL_ADD_LFDNAME                3       /* control type: ring, callback function: lfdname */
#define  PANEL_ADD_WUID                   4       /* control type: ring, callback function: wuid_fonction */
#define  PANEL_ADD_FUNCTION_SELECT        5       /* control type: ring, callback function: select_function */
#define  PANEL_ADD_TIME                   6       /* control type: numeric, callback function: (none) */
#define  PANEL_ADD_RESULT                 7       /* control type: numeric, callback function: (none) */
#define  PANEL_ADD_DURATION               8       /* control type: numeric, callback function: (none) */
#define  PANEL_ADD_BOUTON_INSERT          9       /* control type: command, callback function: insert_step */
#define  PANEL_ADD_SCRIPT_ADD             10      /* control type: command, callback function: addCallback */
#define  PANEL_ADD_QUIT                   11      /* control type: command, callback function: quitCallback */
#define  PANEL_ADD_PARAMETERS             12      /* control type: ring, callback function: parametersCallback */
#define  PANEL_ADD_UNIT                   13      /* control type: string, callback function: (none) */
#define  PANEL_ADD_WUID_VALUE             14      /* control type: string, callback function: (none) */
#define  PANEL_ADD_VALUE                  15      /* control type: string, callback function: (none) */
#define  PANEL_ADD_FORMULA                16      /* control type: string, callback function: (none) */
#define  PANEL_ADD_BOUTONDOT              17      /* control type: command, callback function: boutondot */
#define  PANEL_ADD_BOUTON0                18      /* control type: command, callback function: bouton0 */
#define  PANEL_ADD_INSERT1BIS             19      /* control type: command, callback function: insert1bis */
#define  PANEL_ADD_INSERT1                20      /* control type: command, callback function: insert1 */
#define  PANEL_ADD_INSERT2                21      /* control type: command, callback function: insert2 */
#define  PANEL_ADD_BOUTONADD              22      /* control type: command, callback function: boutonadd */
#define  PANEL_ADD_BOUTON3                23      /* control type: command, callback function: bouton3 */
#define  PANEL_ADD_BOUTON2                24      /* control type: command, callback function: bouton2 */
#define  PANEL_ADD_BOUTONSOU              25      /* control type: command, callback function: boutonsou */
#define  PANEL_ADD_BOUTON1                26      /* control type: command, callback function: bouton1 */
#define  PANEL_ADD_BOUTON6                27      /* control type: command, callback function: bouton6 */
#define  PANEL_ADD_BOUTON5                28      /* control type: command, callback function: bouton5 */
#define  PANEL_ADD_BOUTONMUL              29      /* control type: command, callback function: boutonmul */
#define  PANEL_ADD_BOUTON4                30      /* control type: command, callback function: bouton4 */
#define  PANEL_ADD_BOUTONDEL              31      /* control type: command, callback function: boutondel */
#define  PANEL_ADD_BOUTONDIV              32      /* control type: command, callback function: boutondiv */
#define  PANEL_ADD_BOUTON9                33      /* control type: command, callback function: bouton9 */
#define  PANEL_ADD_BOUTON8                34      /* control type: command, callback function: bouton8 */
#define  PANEL_ADD_BOUTON7                35      /* control type: command, callback function: bouton7 */
#define  PANEL_ADD_BOUTONPAR2             36      /* control type: command, callback function: boutonpar2 */
#define  PANEL_ADD_BOUTONPAR1             37      /* control type: command, callback function: boutonpar1 */
#define  PANEL_ADD_INSERT_PARAMETER2      38      /* control type: command, callback function: Insert_Parameter2 */
#define  PANEL_ADD_INSERT_PARAMETER       39      /* control type: command, callback function: Insert_Parameter1 */
#define  PANEL_ADD_DECORATION             40      /* control type: deco, callback function: (none) */
#define  PANEL_ADD_LABEL_STRING           41      /* control type: string, callback function: (none) */
#define  PANEL_ADD_DECORATION_2           42      /* control type: deco, callback function: (none) */
#define  PANEL_ADD_DECORATION_3           43      /* control type: deco, callback function: (none) */
#define  PANEL_ADD_TEXTMSG4               44      /* control type: textMsg, callback function: (none) */
#define  PANEL_ADD_DECORATION_4           45      /* control type: deco, callback function: (none) */
#define  PANEL_ADD_TEXTMSG5               46      /* control type: textMsg, callback function: (none) */
#define  PANEL_ADD_DECORATION_5           47      /* control type: deco, callback function: (none) */
#define  PANEL_ADD_DECORATION_6           48      /* control type: deco, callback function: (none) */
#define  PANEL_ADD_TEXTMSG6               49      /* control type: textMsg, callback function: (none) */
#define  PANEL_ADD_TEXTMSG                50      /* control type: textMsg, callback function: (none) */
#define  PANEL_ADD_INTERFRAME             51      /* control type: string, callback function: (none) */
#define  PANEL_ADD_TEXTMSG_2              52      /* control type: textMsg, callback function: (none) */
#define  PANEL_ADD_LFPOWER                53      /* control type: string, callback function: (none) */
#define  PANEL_ADD_DECORATION_12          54      /* control type: deco, callback function: (none) */
#define  PANEL_ADD_ACCELERATION           55      /* control type: string, callback function: (none) */
#define  PANEL_ADD_PRESSURE               56      /* control type: string, callback function: (none) */
#define  PANEL_ADD_LEDdebug               57      /* control type: LED, callback function: (none) */
#define  PANEL_ADD_BOX_POST               58      /* control type: radioButton, callback function: box_post */
#define  PANEL_ADD_TIME2                  59      /* control type: string, callback function: (none) */
#define  PANEL_ADD_BOX_PRE                60      /* control type: radioButton, callback function: box_pre */
#define  PANEL_ADD_DURATION2              61      /* control type: string, callback function: (none) */
#define  PANEL_ADD_BOX_SCRIPT             62      /* control type: radioButton, callback function: box_script */

#define  PANEL_MODE                       3
#define  PANEL_MODE_TABLE_VITESSES        2       /* control type: table, callback function: GstTableVitesse */
#define  PANEL_MODE_GRAPHE_VITESSE        3       /* control type: graph, callback function: AffResult */
#define  PANEL_MODE_BUTT_START_VITESSE    4       /* control type: command, callback function: BoutonVitesse */
#define  PANEL_MODE_NBE_CYCLES_VITESSE    5       /* control type: numeric, callback function: (none) */
#define  PANEL_MODE_NUM_CYCLE_VITESSE     6       /* control type: numeric, callback function: (none) */
#define  PANEL_MODE_AFFICH_VITESSE        7       /* control type: numeric, callback function: (none) */
#define  PANEL_MODE_EXP_RESULTS           8       /* control type: table, callback function: exp_results */
#define  PANEL_MODE_SCRIPTS_DETAILS_2     9       /* control type: table, callback function: (none) */
#define  PANEL_MODE_SCRIPTS_DETAILS       10      /* control type: table, callback function: SCRIPTS_DETAILS */
#define  PANEL_MODE_TABLE_SCRIPT          11      /* control type: table, callback function: GstTableRun */
#define  PANEL_MODE_TABLE_PRESSION        12      /* control type: table, callback function: GstTablePression */
#define  PANEL_MODE_GRAPHE_PRESSION       13      /* control type: graph, callback function: (none) */
#define  PANEL_MODE_BUT_START_PRESSION    14      /* control type: command, callback function: BoutonPression */
#define  PANEL_MODE_NBE_CYCLES_PRESSION   15      /* control type: numeric, callback function: (none) */
#define  PANEL_MODE_NUM_CYCLE_PRESSION    16      /* control type: numeric, callback function: (none) */
#define  PANEL_MODE_AFFICH_PRESS          17      /* control type: numeric, callback function: (none) */
#define  PANEL_MODE_SAISIE_VITESSE_MANU   18      /* control type: numeric, callback function: (none) */
#define  PANEL_MODE_SAISIE_DUREE_VIT_MANU 19      /* control type: numeric, callback function: (none) */
#define  PANEL_MODE_SAISIE_PRESSION_MANU  20      /* control type: numeric, callback function: (none) */
#define  PANEL_MODE_SAISIE_DUREE_PRES_MAN 21      /* control type: numeric, callback function: (none) */
#define  PANEL_MODE_COMMENTAIRES          22      /* control type: textBox, callback function: (none) */
#define  PANEL_MODE_MESSAGES_MODE         23      /* control type: textBox, callback function: (none) */
#define  PANEL_MODE_START_ENABLE          24      /* control type: LED, callback function: (none) */
#define  PANEL_MODE_BUTT_START_VIT_PRESS  25      /* control type: command, callback function: BoutonCyclesVitessePress */
#define  PANEL_MODE_BUTT_OUV_GACHE        26      /* control type: command, callback function: OuvertureGache */
#define  PANEL_MODE_BUTT_DETAIL_STATUS    27      /* control type: command, callback function: DetailStatus */
#define  PANEL_MODE_UNIT_PRESSION         28      /* control type: string, callback function: (none) */
#define  PANEL_MODE_UNIT_VITESSE          29      /* control type: string, callback function: (none) */
#define  PANEL_MODE_MessageIndicateur     30      /* control type: textBox, callback function: (none) */
#define  PANEL_MODE_AFFICH_TPS_VIT        31      /* control type: string, callback function: (none) */
#define  PANEL_MODE_AFFICH_TPS_PRESS      32      /* control type: string, callback function: (none) */
#define  PANEL_MODE_BUT_DEBUG2            33      /* control type: command, callback function: butDebug2 */
#define  PANEL_MODE_BUT_DEBUG1            34      /* control type: command, callback function: butDebug1 */
#define  PANEL_MODE_BUTTON_ANALYSE        35      /* control type: command, callback function: button_analyse */
#define  PANEL_MODE_BUT_START_RUN         36      /* control type: command, callback function: BoutonExecution */
#define  PANEL_MODE_TOTALTIME             37      /* control type: numeric, callback function: (none) */
#define  PANEL_MODE_ENDTIME               38      /* control type: numeric, callback function: (none) */
#define  PANEL_MODE_WUIDDIAG2             39      /* control type: string, callback function: (none) */
#define  PANEL_MODE_WUIDDIAG1             40      /* control type: string, callback function: (none) */
#define  PANEL_MODE_WUIDSTAND2            41      /* control type: string, callback function: (none) */
#define  PANEL_MODE_WUIDSTAND1            42      /* control type: string, callback function: (none) */
#define  PANEL_MODE_INDICATOR_SET_LOG_DIR 43      /* control type: string, callback function: (none) */
#define  PANEL_MODE_INDICATOR_TESTFOLDER  44      /* control type: string, callback function: (none) */
#define  PANEL_MODE_INDICATOR_TESTSCRIPT  45      /* control type: string, callback function: (none) */
#define  PANEL_MODE_INDICATOR_DATABASE    46      /* control type: string, callback function: (none) */
#define  PANEL_MODE_INDICATOR_CONFIG_ANUM 47      /* control type: string, callback function: (none) */
#define  PANEL_MODE_BUTTON_ANUM           48      /* control type: command, callback function: button_anum */
#define  PANEL_MODE_BUTTON_TESTFOLDER     49      /* control type: command, callback function: button_testfolder */
#define  PANEL_MODE_BUTTON_SET_LOG_DIR    50      /* control type: command, callback function: button_set_log_dir */
#define  PANEL_MODE_BUTTON_TESTSCRIPT     51      /* control type: command, callback function: button_testscript */
#define  PANEL_MODE_BUTTON_DATABASE       52      /* control type: command, callback function: button_database */
#define  PANEL_MODE_BUTTON_SEE_GRAPH      53      /* control type: command, callback function: button_see_graph */
#define  PANEL_MODE_DECORATION_10         54      /* control type: deco, callback function: (none) */
#define  PANEL_MODE_DECORATION            55      /* control type: deco, callback function: (none) */
#define  PANEL_MODE_DECORATION_2          56      /* control type: deco, callback function: (none) */
#define  PANEL_MODE_DECORATION_4          57      /* control type: deco, callback function: (none) */
#define  PANEL_MODE_PICTURE               58      /* control type: picture, callback function: littlemsg */
#define  PANEL_MODE_DECORATION_3          59      /* control type: deco, callback function: (none) */
#define  PANEL_MODE_DECORATION_5          60      /* control type: deco, callback function: (none) */
#define  PANEL_MODE_DECORATION_6          61      /* control type: deco, callback function: (none) */
#define  PANEL_MODE_TESTSCRIPT            62      /* control type: textMsg, callback function: (none) */
#define  PANEL_MODE_EXPRESULTSC           63      /* control type: textMsg, callback function: (none) */
#define  PANEL_MODE_SCRIPTSEQEXE          64      /* control type: textMsg, callback function: (none) */
#define  PANEL_MODE_TEXTMSG_WUID          65      /* control type: textMsg, callback function: (none) */
#define  PANEL_MODE_TEXTMSG1              66      /* control type: textMsg, callback function: (none) */
#define  PANEL_MODE_CHECKRESULTS          67      /* control type: textMsg, callback function: (none) */
#define  PANEL_MODE_ONLINESEQ             68      /* control type: textMsg, callback function: (none) */
#define  PANEL_MODE_TEXTMSG2              69      /* control type: textMsg, callback function: (none) */
#define  PANEL_MODE_DECORATION_7          70      /* control type: deco, callback function: (none) */
#define  PANEL_MODE_ENDTIMESEQUENCE       71      /* control type: numeric, callback function: (none) */
#define  PANEL_MODE_FAILEDSEQUENCE        72      /* control type: textBox, callback function: (none) */
#define  PANEL_MODE_CLOCK_ANALYSE         73      /* control type: picture, callback function: (none) */
#define  PANEL_MODE_LED3                  74      /* control type: LED, callback function: (none) */
#define  PANEL_MODE_LED2                  75      /* control type: LED, callback function: (none) */
#define  PANEL_MODE_LED1                  76      /* control type: LED, callback function: (none) */
#define  PANEL_MODE_SAVEFILEBUTTON        77      /* control type: pictButton, callback function: savefilesbutton */
#define  PANEL_MODE_LEDdegub2             78      /* control type: LED, callback function: (none) */
#define  PANEL_MODE_PROGRESSBARSEQ        79      /* control type: scale, callback function: (none) */
#define  PANEL_MODE_PROGRESSBAR           80      /* control type: scale, callback function: (none) */
#define  PANEL_MODE_SHOW_ACCEL2           81      /* control type: numeric, callback function: (none) */
#define  PANEL_MODE_SHOW_PRES2            82      /* control type: numeric, callback function: (none) */
#define  PANEL_MODE_SHOW_ACC              83      /* control type: scale, callback function: (none) */
#define  PANEL_MODE_SHOW_PRESS            84      /* control type: scale, callback function: (none) */
#define  PANEL_MODE_TEXTMSG               85      /* control type: textMsg, callback function: (none) */
#define  PANEL_MODE_TEXTMSG_9             86      /* control type: textMsg, callback function: (none) */
#define  PANEL_MODE_DECORATION_8          87      /* control type: deco, callback function: (none) */
#define  PANEL_MODE_DECORATION_9          88      /* control type: deco, callback function: (none) */
#define  PANEL_MODE_TEXTMSG_8             89      /* control type: textMsg, callback function: (none) */
#define  PANEL_MODE_MODE_ANALYSE          90      /* control type: textButton, callback function: mode_analyse */
#define  PANEL_MODE_MODE_EXE              91      /* control type: textButton, callback function: mode_exe */
#define  PANEL_MODE_MODE_CREATION         92      /* control type: textButton, callback function: mode_creation */
#define  PANEL_MODE_LED32                 93      /* control type: color, callback function: (none) */
#define  PANEL_MODE_LED22                 94      /* control type: color, callback function: (none) */
#define  PANEL_MODE_LED12                 95      /* control type: color, callback function: (none) */
#define  PANEL_MODE_START_ENABLE2         96      /* control type: color, callback function: (none) */
#define  PANEL_MODE_TEXTBOX_ANALYSIS      97      /* control type: textBox, callback function: (none) */
#define  PANEL_MODE_QUITBUTTON            98      /* control type: command, callback function: redCross */
#define  PANEL_MODE_COMMANDBUTTON         99      /* control type: command, callback function: InitConfig */

#define  PANEL_STAT                       4
#define  PANEL_STAT_DECORATION            2       /* control type: deco, callback function: (none) */
#define  PANEL_STAT_VARIATOR_COMM         3       /* control type: LED, callback function: (none) */
#define  PANEL_STAT_USB6008_COMM          4       /* control type: LED, callback function: (none) */
#define  PANEL_STAT_ENGINE_RUNNING        5       /* control type: LED, callback function: (none) */
#define  PANEL_STAT_EMERGENCY_STOP        6       /* control type: LED, callback function: (none) */
#define  PANEL_STAT_SECURITY_LOOP         7       /* control type: LED, callback function: (none) */
#define  PANEL_STAT_SECURITY_HOOD         8       /* control type: LED, callback function: (none) */
#define  PANEL_STAT_DOOR_2                9       /* control type: LED, callback function: (none) */
#define  PANEL_STAT_DOOR_1                10      /* control type: LED, callback function: (none) */
#define  PANEL_STAT_SPEED                 11      /* control type: numeric, callback function: (none) */
#define  PANEL_STAT_PRESSURE              12      /* control type: numeric, callback function: (none) */
#define  PANEL_STAT_USB6008               13      /* control type: textMsg, callback function: (none) */
#define  PANEL_STAT_DECORATION_2          14      /* control type: deco, callback function: (none) */
#define  PANEL_STAT_VARIATOR              15      /* control type: textMsg, callback function: (none) */
#define  PANEL_STAT_COMMANDBUTTON         16      /* control type: command, callback function: CloseStatusPanel */
#define  PANEL_STAT_MESSAGES              17      /* control type: textBox, callback function: (none) */

#define  PANELGRAPH                       5
#define  PANELGRAPH_GRAPH                 2       /* control type: graph, callback function: AffResult2 */
#define  PANELGRAPH_SHOW_PRES             3       /* control type: numeric, callback function: (none) */
#define  PANELGRAPH_SHOW_ACCEL            4       /* control type: numeric, callback function: (none) */
#define  PANELGRAPH_QUITBUTTON            5       /* control type: command, callback function: (none) */

#define  PANELVALUE                       6
#define  PANELVALUE_SEQUENCEVALUE         2       /* control type: toggle, callback function: sequencevalue */
#define  PANELVALUE_RANGEVALUE            3       /* control type: toggle, callback function: rangevalue */
#define  PANELVALUE_ONEVALUE              4       /* control type: toggle, callback function: onevalue */
#define  PANELVALUE_OKBUTTON              5       /* control type: command, callback function: okbutton */

#define  POPUP_ADD                        7
#define  POPUP_ADD_CHECK_AFTER            2       /* control type: radioButton, callback function: GstCheck */
#define  POPUP_ADD_BUTT_OK                3       /* control type: command, callback function: GstOptAddOk */
#define  POPUP_ADD_CHECK_BEFORE           4       /* control type: radioButton, callback function: GstCheck */
#define  POPUP_ADD_BUTT_CANCEL            5       /* control type: command, callback function: GstOptAddCancel */
#define  POPUP_ADD_NB_ROWS                6       /* control type: numeric, callback function: (none) */


     /* Control Arrays: */

#define  CTRLARRAY                        1
#define  CTRLARRAY_2                      2
#define  CTRLARRAY_3                      3
#define  CTRLARRAY_4                      4

     /* Menu Bars, Menus, and Menu Items: */

#define  CONFIG                           1
#define  CONFIG_QUITTER                   2       /* callback function: Quitter */

#define  GST_RUN                          2
#define  GST_RUN_MAIN                     2
#define  GST_RUN_MAIN_ADD                 3
#define  GST_RUN_MAIN_INSERT              4
#define  GST_RUN_MAIN_DELETE              5
#define  GST_RUN_MAIN_DELETEALL           6
#define  GST_RUN_MAIN_SEPARATOR           7
#define  GST_RUN_MAIN_MOVE_UP             8
#define  GST_RUN_MAIN_MOVE_DOWN           9
#define  GST_RUN_MAIN_SELECT              10
#define  GST_RUN_MAIN_SEPARATOR_2         11
#define  GST_RUN_MAIN_LOADSEQ             12

#define  GST_SCRIPT                       3
#define  GST_SCRIPT_MAIN                  2
#define  GST_SCRIPT_MAIN_ADD              3
#define  GST_SCRIPT_MAIN_INSERT           4
#define  GST_SCRIPT_MAIN_MODIFY           5
#define  GST_SCRIPT_MAIN_SEPARATOR_2      6
#define  GST_SCRIPT_MAIN_MOVE_UP          7
#define  GST_SCRIPT_MAIN_MOVE_DOWN        8
#define  GST_SCRIPT_MAIN_SEPARATOR_3      9
#define  GST_SCRIPT_MAIN_DELETE           10
#define  GST_SCRIPT_MAIN_DELETEALL        11
#define  GST_SCRIPT_MAIN_SEPARATOR        12
#define  GST_SCRIPT_MAIN_LOAD_TEST_SCRIPT 13

#define  GST_TAB                          4
#define  GST_TAB_GST                      2
#define  GST_TAB_GST_ADD_1                3
#define  GST_TAB_GST_SUPPR                4
#define  GST_TAB_GST_SEPARATOR            5
#define  GST_TAB_GST_COPIER               6
#define  GST_TAB_GST_COLLER               7
#define  GST_TAB_GST_INSERT               8
#define  GST_TAB_GST_INSERT_AFTER         9
#define  GST_TAB_GST_SEPARATOR_2          10
#define  GST_TAB_GST_AGRANDIR             11
#define  GST_TAB_GST_RESTAURER            12

#define  MENUEXP                          5
#define  MENUEXP_MAIN                     2
#define  MENUEXP_MAIN_ADD                 3
#define  MENUEXP_MAIN_INSERT              4
#define  MENUEXP_MAIN_MODIFY              5
#define  MENUEXP_MAIN_SEPARATOR_3         6
#define  MENUEXP_MAIN_MOVEUP              7
#define  MENUEXP_MAIN_MOVEDOWN            8
#define  MENUEXP_MAIN_SEPARATOR           9
#define  MENUEXP_MAIN_DELETE              10
#define  MENUEXP_MAIN_DELETEALL           11
#define  MENUEXP_MAIN_SEPARATOR_2         12
#define  MENUEXP_MAIN_LOADEXPECTED        13


     /* Callback Prototypes: */

int  CVICALLBACK addCallback(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK addexp(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK addtol(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK addtol_2(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK addval(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK AffResult(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK AffResult2(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK bouton0(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK bouton1(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK bouton2(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK bouton3(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK bouton4(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK bouton5(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK bouton6(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK bouton7(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK bouton8(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK bouton9(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK boutonadd(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK BoutonCyclesVitessePress(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK boutondel(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK boutondiv(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK boutondot(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK BoutonExecution(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK boutonmul(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK boutonpar1(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK boutonpar2(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK BoutonPression(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK boutonsou(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK BoutonVitesse(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK box_post(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK box_pre(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK box_script(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK btntolprct(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK btntolval(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK but0(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK but1(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK but2(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK but3(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK but4(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK but5(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK but6(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK but7(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK but8(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK but9(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK butA(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK butB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK butC(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK butD(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK butDebug1(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK butDebug2(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK butdiv(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK butdot(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK butE(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK butF(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK butmult(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK butpar1(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK butpar2(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK butsub(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK butsum(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK button_analyse(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK button_anum(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK button_database(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK button_see_graph(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK button_set_log_dir(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK button_testfolder(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK button_testscript(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK CloseStatusPanel(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK deletebutton(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK deltol(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK deltolper(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK delvalue(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK DetailStatus(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK exp_results(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK exp_value(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK expected_list(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK fieldtocheck(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK GstCheck(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK GstOptAddCancel(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK GstOptAddOk(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK GstTablePression(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK GstTableRun(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK GstTableVitesse(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK InitConfig(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK insert1(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK insert1bis(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK insert2(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK Insert_Parameter1(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK Insert_Parameter2(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK insert_step(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK insertformule(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK lfdname(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK lfdnbframe(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK littlemsg(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK mode_analyse(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK mode_creation(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK mode_exe(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK okbutton(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK OkCallback(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK onevalue(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK OuvertureGache(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK parametersCallback(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK parametersdata(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK quitCallback(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
void CVICALLBACK Quitter(int menubar, int menuItem, void *callbackData, int panel);
int  CVICALLBACK rangevalue(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK redCross(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK savefilesbutton(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK SCRIPTS_DETAILS(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK select_function(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK selectparam(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK sequencevalue(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK wuid_fonction(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK wuid_fonction_exp(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);


#ifdef __cplusplus
    }
#endif
