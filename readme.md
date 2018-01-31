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


Cet ordinateur est basé sur un MCU PIC32MX170F256B

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

