//README - 11/09/2020 

Ce dossier contient 4 sous dossier :
- Exemple Installation: avec ACASYA.EXE et les fichiers et dossiers n�cessaires au bon fonctionnement d'ACASYA (dll, config..)
- Screenshots : capture d'�crans de la version actuelle et des anciennes versions 
- User Manual : l'User Manual en format PDF et en version Word (modifiable)
- Autres : avec un exemple de config ANUm, de Database et de script test. 

et deux fichiers: 
- ACASYA-setup-1.0.exe : fichier � executer pour installer ACASYA sur l'ordinateur avec son environnement. C'est l'installateur d'ACASYA. 
- acasya_icon.ico


//D�tails sur le dossier "Exemple Installation" *****************************************
Le dossier contient le fichier executable g�n�r� par LabWindow CVI et tous les fichiers et dossier n�cessaires 
au bon foncttionnement du logiciel (dll, config...)

Ces fichiers et le dossier Config doivent �tre pr�sents avec l'executable ACASYA.exe pour assurer le bon fonctionnement de l'application. 


//Cr�ation d'un installateur ACASYA-setup.exe*****************************************

1. Telecharger et lancer le logiciel Inno Setup 6.

2. File > New 

3. Cliquer sur "Next"

4. Indiquer les informations de l'application � installer 

5. Cliquer sur "Next"

6. Choisir la destination de l'installation (Program Files) et le nom de dossier d'installation ("ACASYA)

7. Cliquer sur "Next"

8. Choisir le fichier executable : fichier ACASYA.exe g�n�r� par LabWindow CVI 

9. Dans la section "Other application files:", ajouter tous les fichiers pr�sents dans 
   le dossier Exemple Installation (m�me le fichier Config.ini dans le sous dossier Config).

10. Dans la m�me fen�tre de Inno Setup, cliquer sur le fichier Config.ini et cliquer sur "Edit..."
    Indiquer "Config" dans le champ "Destination subfolder". Puis faites OK. 

11. Cliquer sur "Next" jusqu'� la fen�tre "Setup Install Mode"

11. Cliquer sur "Non admininistrative install mode". 

11. Cliquer sur "Next" jusqu'� la fen�tre "Compiler Settings"

12. Indiquer le dossier dans lequel stocker l'installateur

13. Indiquer "ACASYA-setup" dans le champ "Compiler output base file name"

14. Indiquer l'icon de ACASYA, pr�sent ici. 

15. Cliquer sur "Next" jusqu'� la fen�tre "Finish". Cliquer sur "Finish"

16. A la question "Would you like to compile the new script now ?", r�pondre "Oui". 

17. R�pondre "Oui" � la question "Would you like to save the script before compiling?" et choisir le dossier de sauvegarde.

18. La cr�ation de l'executable est termin�e. 


//REMARQUES GLOBALES *****************************************

- Lors du tout premier lancement de l'application, le choix des fichiers de config et du dossier de travail peut �tre un peu long. 

- Pour lancer le projet dans CVI, il faut aller dans le dossier source MaximePAGES puis Dossiers Personnels > ACASYA project > puis lancer acasya_project.prj

- Dans le dossier "Dossiers Personnels", le tableau de bord de mon stage est enregistr� en PDF (Conti_Stage2020).