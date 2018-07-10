VPC-32v
=======

Le projet VPC-32vga Est la version VGA de l'ordinateur [VPC-32](https://github.com/picatout/vpc-32). Donc au lieu d'avoir une sortie NTSC/PAL l'ordinateur a une sortie pour moniteur VGA.

sortie vidéo:
-------------

mode texte

30 lignes de 80 caractères

mode graphique:
---------------

480x240 pixels monochrome

détails matériel:
----------------

 1. processeur principal MCU PIC32MX170F256B
 2. interface utilisateur locale ou remote.
 3. Interface locale par clavier et moniteur VGA
 4. Interface remote via le port sériel émulation VT-100.
 5. Carte SD pour la sauvegarde des fichiers. Système FAT32.
 6. RTCC MCP7940N. conserve l'heure et la date même lorsque l'ordinateur est éteint.
 7. Possibilité de définir 2 alarmes sur le RTCC.
 8. Processeur clavier PIC12F1572. Gère le clavier en mode PS/2 et communique en RS-232 avec le PIC32MX.
 9. Mémoire RAM supplémentaire de 64Ko sous la forme d'un 23LC512.
 10. Ampli audio incorporé sous la forme d'un LM386.
 

détails logiciel:
-----------------
1. Shell de commande pour la gestion des fichiers, des alarmes et autres fonctions.
2. interpréteur BASIC procédural interactif.
3. Un programme BASIC peut-être lancer à partir du shell de commande simplement en invoquant le nom du fichier.
4. À faire: développer un API de programmation pour les programmes binaire natif qui s'éxécuteraient à partir de la RAM.

Interpréteur BASIC:
-------------------
J'ai démarrer à partir du code source du projet [pv16sog](https://picatout.github.io/pv16sog/) mais les fonctionnalités ont étées
étendues et le code a subies de nombreuses modifications. Le compilateur génère toujours du bytecode qui s'exécute sur une machine
virtuelle écrite en assembleur. Comme il s'agit d'un processeur différent du projet **pv16sog** la machine virtuelle est évidemment
complètement réécrite.

Il s'agit d'un dialecte BASIC procédural et qui supporte les types de données suivants:

1. **BYTE** entier 8 bits non signés.
2. **INTEGER** entier 32 bits signés.
3. **FLOAT** nombre à virgule flottante 32 bits.
4. **STR** chaîne de caractère ASCII terminée par un zéro.

Pour le moment seul les tableaux à 1 dimension sont supportées.

Les références avant des SUB et FUNC sont permises si elles sont préalablement déclarées avec le mot réservé **DECLARE**.


LICENCE:
--------

Copyright 2013,2014,2015,2017,2018 Jacques Deschênes

L'ensemble du projet logiciel sauf ce qui est sous droits réservés par Microchip ou autre ayant droit, est sous GPLv3
Lire copying.txt  au  sujet de GPLv3.

Le projet schématique KiCad est sous licence Creative Commons  CC BY-SA
Lire à ce sujet: http://creativecommons.org/licenses/by-sa/2.0/legalcode

jd_temp@yahoo.fr


modifications:
--------------

L'interface clavier est maintenant assurée par un PIC12F1572 selon le projet [ps2_rs232](https://github.com/picatout/ps2_rs232). 

La combinaison de touches CTRL-ALT-DEL réinitialise le processeur principal par un signal hardware provenant du PIC12F1572.

J'ai l'intention d'apporter des modifications majeures à la partie logiciel du projet.

schématique
-----------
circuit révisé en date du 2018-01-31
![schématique](/docs/vpc-32vga_schematic.png)

montage
-------
![carte prototype](/docs/prototype/carte.png)
![boitier face avant](/docs/prototype/boitier face.png)
![boitier arrière](/docs/prototype/boitier arrière.png)
