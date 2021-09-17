//README - 11/09/2020 

Ce dossier contient 4 sous dossier :
- Exemple Installation: avec ACASYA.EXE et les fichiers et dossiers nécessaires au bon fonctionnement d'ACASYA (dll, config..)
- Screenshots : capture d'écrans de la version actuelle et des anciennes versions 
- User Manual : l'User Manual en format PDF et en version Word (modifiable)
- Autres : avec un exemple de config ANUm, de Database et de script test. 

et deux fichiers: 
- ACASYA-setup-1.0.exe : fichier à executer pour installer ACASYA sur l'ordinateur avec son environnement. C'est l'installateur d'ACASYA. 
- acasya_icon.ico


//Détails sur le dossier "Exemple Installation" *****************************************
Le dossier contient le fichier executable généré par LabWindow CVI et tous les fichiers et dossier nécessaires 
au bon foncttionnement du logiciel (dll, config...)

Ces fichiers et le dossier Config doivent être présents avec l'executable ACASYA.exe pour assurer le bon fonctionnement de l'application. 


//Création d'un installateur ACASYA-setup.exe*****************************************

1. Telecharger et lancer le logiciel Inno Setup 6.

2. File > New 

3. Cliquer sur "Next"

4. Indiquer les informations de l'application à installer 

5. Cliquer sur "Next"

6. Choisir la destination de l'installation (Program Files) et le nom de dossier d'installation ("ACASYA)

7. Cliquer sur "Next"

8. Choisir le fichier executable : fichier ACASYA.exe généré par LabWindow CVI 

9. Dans la section "Other application files:", ajouter tous les fichiers présents dans 
   le dossier Exemple Installation (même le fichier Config.ini dans le sous dossier Config).

10. Dans la même fenêtre de Inno Setup, cliquer sur le fichier Config.ini et cliquer sur "Edit..."
    Indiquer "Config" dans le champ "Destination subfolder". Puis faites OK. 

11. Cliquer sur "Next" jusqu'à la fenêtre "Setup Install Mode"

11. Cliquer sur "Non admininistrative install mode". 

11. Cliquer sur "Next" jusqu'à la fenêtre "Compiler Settings"

12. Indiquer le dossier dans lequel stocker l'installateur

13. Indiquer "ACASYA-setup" dans le champ "Compiler output base file name"

14. Indiquer l'icon de ACASYA, présent ici. 

15. Cliquer sur "Next" jusqu'à la fenêtre "Finish". Cliquer sur "Finish"

16. A la question "Would you like to compile the new script now ?", répondre "Oui". 

17. Répondre "Oui" à la question "Would you like to save the script before compiling?" et choisir le dossier de sauvegarde.

18. La création de l'executable est terminée. 


//REMARQUES GLOBALES *****************************************

- Lors du tout premier lancement de l'application, le choix des fichiers de config et du dossier de travail peut être un peu long. 

- Pour lancer le projet dans CVI, il faut aller dans le dossier source MaximePAGES puis Dossiers Personnels > ACASYA project > puis lancer acasya_project.prj

- Dans le dossier "Dossiers Personnels", le tableau de bord de mon stage est enregistré en PDF (Conti_Stage2020).