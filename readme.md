VPC-32v
=======

Le projet VPC-32vga Est la version VGA de l'ordinateur [VPC-32](https://github.com/picatout/vpc-32). Donc au lieu d'avoir une sortie NTSC/PAL l'ordinateur a une sortie pour moniteur VGA.

sortie vid�o:
-------------

mode texte

30 lignes de 80 caract�res

mode graphique:
---------------

480x240 pixels monochrome

d�tails mat�riel:
----------------

 1. processeur principal MCU PIC32MX170F256B
 2. interface utilisateur locale ou remote.
 3. Interface locale par clavier et moniteur VGA
 4. Interface remote via le port s�riel �mulation VT-100.
 5. Carte SD pour la sauvegarde des fichiers. Syst�me FAT32.
 6. RTCC MCP7940N. conserve l'heure et la date m�me lorsque l'ordinateur est �teint.
 7. Possibilit� de d�finir 2 alarmes sur le RTCC.
 8. Processeur clavier PIC12F1572. G�re le clavier en mode PS/2 et communique en RS-232 avec le PIC32MX.
 9. M�moire RAM suppl�mentaire de 64Ko sous la forme d'un 23LC512.
 10. Ampli audio incorpor� sous la forme d'un LM386.
 

d�tails logiciel:
-----------------
1. Shell de commande pour la gestion des fichiers, des alarmes et autres fonctions.
2. interpr�teur BASIC proc�dural interactif.
3. Un programme BASIC peut-�tre lancer � partir du shell de commande simplement en invoquant le nom du fichier.
4. � faire: d�velopper un API de programmation pour les programmes binaire natif qui s'�x�cuteraient � partir de la RAM.

Interpr�teur BASIC:
-------------------
J'ai d�marrer � partir du code source du projet [pv16sog](https://picatout.github.io/pv16sog/) mais les fonctionnalit�s ont �t�es
�tendues et le code a subies de nombreuses modifications. Le compilateur g�n�re toujours du bytecode qui s'ex�cute sur une machine
virtuelle �crite en assembleur. Comme il s'agit d'un processeur diff�rent du projet **pv16sog** la machine virtuelle est �videmment
compl�tement r��crite.

Il s'agit d'un dialecte BASIC proc�dural et qui supporte les types de donn�es suivants:

1. **BYTE** entier 8 bits non sign�s.
2. **INTEGER** entier 32 bits sign�s.
3. **FLOAT** nombre � virgule flottante 32 bits.
4. **STR** cha�ne de caract�re ASCII termin�e par un z�ro.

Pour le moment seul les tableaux � 1 dimension sont support�es.

Les r�f�rences avant des SUB et FUNC sont permises si elles sont pr�alablement d�clar�es avec le mot r�serv� **DECLARE**.


LICENCE:
--------

Copyright 2013,2014,2015,2017,2018 Jacques Desch�nes

L'ensemble du projet logiciel sauf ce qui est sous droits r�serv�s par Microchip ou autre ayant droit, est sous GPLv3
Lire copying.txt  au  sujet de GPLv3.

Le projet sch�matique KiCad est sous licence Creative Commons  CC BY-SA
Lire � ce sujet: http://creativecommons.org/licenses/by-sa/2.0/legalcode

jd_temp@yahoo.fr


modifications:
--------------

L'interface clavier est maintenant assur�e par un PIC12F1572 selon le projet [ps2_rs232](https://github.com/picatout/ps2_rs232). 

La combinaison de touches CTRL-ALT-DEL r�initialise le processeur principal par un signal hardware provenant du PIC12F1572.

J'ai l'intention d'apporter des modifications majeures � la partie logiciel du projet.

sch�matique
-----------
circuit r�vis� en date du 2018-01-31
![sch�matique](/docs/vpc-32vga_schematic.png)

montage
-------
![carte prototype](/docs/prototype/carte.png)
![boitier face avant](/docs/prototype/boitier face.png)
![boitier arri�re](/docs/prototype/boitier arri�re.png)
