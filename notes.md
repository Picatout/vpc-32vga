2018-04-09
----------
1. Corrigé bogue dans code_let_string().
2. tester BASIC.
3. Corriger bogue dans INPUT
4. Corriger bogue dans fonction booléenne.
5. Ajout fonctiopns booléenne bool_not,bool_or,bool_and  dans VM.

2018-04-08
----------
1. Travail sur vpcBASIC.
2. Correction code_let_string().
3. Modification à la gestion des erreurs du compilateur et de la VM.
4. Retravailler la boucle read-compile-execute

2018-04-07
----------
1. Travail sur vpcBASIC.
2. Changement au saut relatif. Au départ je pensait que codage des saut relatif
   sur 1 octet {-128...127} serait suffisant. En pratique il s'est avéré insuffisant
   je vais donc l'étrendre à 2 octets {-32768..32767}. Il faut donc modifié la machine
   virtuelle et le compilateur en conséquence. 
3. Modification à la **VM** pour que trace indique le nom symbolique au lieu du opcode.
   la macro _dict ajout le nom symbolique avant chaque opcode de la machine virtuelle
   et **print_state** affiche ce nom.

2018-04-06
----------
1. Travail sur vpcBASIC.
2. Déboguer mécanisme d'appel des fonctions et sous-routines. 

2018-04-05
----------
1. Travail sur vpcBASIC.
2. Modifications pour tenir compte de la nouvelle définition de la structure **var_t**
   Les champes **ro**, **array** et **dim** sont maintenant utilisés. L'énumération **var_type_e**
   a été modifiée en conséquence.
3. bogue à corrigée: ? 4*f(n) imprime 0.


2018-04-04
----------
1. Travail sur vpcBASIC. Ajout des fonctions d'accès à la mémoire SPI RAM.
2. Ajout de SPRITE,SRCLEAR,SRREAD,SRWRITE,SRSAVE,SRLOAD,POLYGON.

2018-04-03
----------
1. Travail sur vpcBASIC. Ajout des fonctions graphique dans vm.S


2018-04-01
----------
1. Travail sur vpcBASIC.
2. Ajout dans vm.S, **extbit**,**setcursor**,**extbit**,**settimer**,**qtimeout**
3. travail sur LOCAL, correction bogue dans **vm:lc_space**
4. Ajout de **vm:inv_vid**
5. Ajout de la fonction insert_line() à la console.
6. Nettoyage vms

2018-03-31
----------
1. Travail sur vpcBASIC.
2. Modification du type **var_t** pour besoins futur.
3. ajout eVAR_CONST_STR dans dans l'énumération var_type_e
4. Modifications dans kw_const() et kw_let() pour tenir compte de eVAR_CONST_STR
5. Ajouté  **curline** et **curpos** dans vm.S ainsi que macro **_console**


2018-03-30
----------
1. Travail sur vpcBASIC
2. Travail bogue commande INPUT. INPUT maintenant fonctionnel
3. modification à allocation des chaînes, maintenant allouée dyanmiquement sur le heap.
4. correcion bogue fonction MIN et MAX.
5: tests de vitesse:
    boucle FOR ... NEXT vide: 100 000 boucles prend 521msec.
    boucle WHILE ... WEND avec incrément de comtpeur sur 1 seconde: 112366 itérations 

2018-03-29
----------
1. Travail sur vpcBASIC et vm.S
2. Travail sur INPUT

2018-03-28
----------
1. Travail sur vpcBASIC et vm.S
2. Problème avec SUB et FUNC résolue.
3. Modification impression chaîne litérale. Remplacement de chaîne comptée par ASCIIZ.
   Renommé dotq par prt_str dans vm.S.
4. Travail sur INPUT


2018-03-27
----------
1. Travail sur vm.S, ajout de dots, depth et modfication de next, set_frame, leave
2. Travail sur vpcBASIC modification compilation concernant SUB et FUNC.
3. ***** A résoudre  problème de la valeur de retour des FUNC.

2018-03-26
----------
1. travail sur machine virtuelle. Changer le fonctionnement des piles. Elles étaient
pré-incémentées, maintenant elles sont post-incrémentées.
2. Suppression des mots suivants dans la vm: wlit,docol,exit,exec,umsmod,msmod
2018-03-24
----------
1. modification à la machine virtuelle vm.
2. travail sur kw_func et kw_sub 

2018-03-24
----------
1. Travail sur vpcBASIC. 
2. renommé fix_branch_address() en fix_fore_jump()
3. créer fix_back_jump()
4. Travail sur SELECT CASE
5. Modifié code VM pour IBRA

2018-03-23
----------
1. Travail sur interpréteur BASIC et la machine virtuelle. Les contrôle de flux.
2. Ajout de vt_read_line(), modification de read_line(), ser_read_line() et kbd_read_line()
3. Travail sur structure FOR...NEXT

2018-03-22
----------
1. Travail sur interpréteur BASIC et la machine virtuelle.


2018-03-21
----------
1. Travail sur interpréteur BASIC et la machine virtuelle.


2018-03-17
----------
1. Travail sur interpréteur BASIC et la machine virtuelle.


2018-03-17
----------
1. Travail sur interpréteur BASIC et la machine virtuelle.
2. diskio:get_fattime()  ajout du code pour VPC_32VGA
3. Correction bogue dans console:clear_line()
4. Correction bogue dans vga:vga_clear_line()

2018-03-16
----------
1. Travail sur serial_comm.c, serial_rx_isr(void):réduction limite avant d'envoyer XOFF.
2. Modification de console:clear_line().
3. Implémentation de vga_clear_line() et modification de vt_clear_line()
4. Commencer travail sur machine virtuelle à piles vms.S

2018-03-15
----------
1. Travail sur serial_comm.c
2. Modification au projet ps2_rs232 pour ajouter touche virtuelle VK_SDEL=191
3. Modification à vt100.c et keyboard.h pour ajouter touche VK_SDEL.
4. Modificationà l'éditeur pour remplacé VK_CBACK par VK_SDEL comme touche effacement début ligne. 

2018-03-14
----------
1. Travail sur serial_comm.c, ajout interruption et tampon de réception. 
2. Modification keyboard:kbd_rx_isr() pour ajouter gestion d'erreur sur réception.


2018-03-13
----------
1. Travail sur vt100.c


2018-03-11
----------
1. Travail sur editor.c

2018-03-10
----------
1. Travail sur editor.c
2. Ajout de spiram:sram_move()

2018-03-09
----------
1. Travail sur editor.c
2. Ajout de console:scroll_up()
3. A faire: travail sur delete_at();

2018-03-08
----------
1. Travail sur editor.c
2. ajouter  kw_run, modification pour supporté la commande RUN.


2018-03-07
----------
1. Travail sur editor.c
2. ajout de console:set_tab_width(),vt100:vt_set_tab_width() et vga:vga_set_tab_widh()
3. Suppression de console:set_auto_scroll(),vt100:vt_set_auto_scroll()
4. Corrigé erreur dans vga_put_char() pour le traitement du caractère '\t'.

2018-03-06
----------
1. Travail sur editor.c

2018-03-03
----------
1. Travail sur éditeur de texte.
2. Ajout fonction editor:invert_display()

2018-03-02
----------
1. Retraivaillé l'éditeur de texte. 
2. Corrigée bogue dans HardwareProfile:free_heap();
3. Ajout de la fonction HardwareProfile:biggest_chunk() qui retourne le plus gros
   morceau de mémoire RAM allouable par malloc().

2018-02-28
----------
1. Création branche new_editor pour travail dans editor.c


2018-02-27
----------
1. Travail sur shell. Corrigé bogue et ajouté commande 'con'.
2. Modification à ser_read_line().

2018-02-26
----------
1. Travail sur shell. bogue sur variable nom longueur 1.

2018-02-25
----------
1. Travail sur new_shell.

2018-02-24
-----------
1. Création de la branche new_shell.
2. Refonte majeure de shell.c Toute commande est une foncion qui retourne une
   chaîne de caractère. Le shell imprime la chaîne retournée par la commande et
   libère la mémoire réservée pour cette châine.

2018-02-23
----------
1. Travail sur shell.c

2018-02-21
----------
1. Travail sur shell.c
2. modification organisation des commandes en un tableau de type command_t
3. ajout de la commande 'set' et substitution de variables.

2018-02-19
----------
1. Uniformisation des noms de fonctions d'interface périphériques

2018-02-18
-----------
1.  Transfert des fonctions lecture clavier de vga.c vers keyboard.c

2018-02-17
----------
1. transférer les fonctions relié à l'affichage vidéo vga de console.c vers vga.c

2018-02-16
----------
1. Réorganisation du projet. Pour le moment on enlève du projet les modules shell,
    console, vm, editor, graphics, vt100, pour se concentrer sur le coeur du système
    d'exploitation. Il faut developper un API.

2018-02-13
----------
1. travail sur console pour créer une interface commune entre console locale et rs-232.


2018-02-12
----------
1. évaluation du temps CPU pris par les interruptions.

2018-02-11
----------
1. Ajout commande **clktrim** dans le shell.
2. Ajout commande **echo** dans shell.
3. Ajout function **spaces()** dans module console. 

2018-02-10
-----------
1. Modification à print_hex() et print_int();
2. Ajout message au démarrage du shell pour indiquer la date et heure dernier power down.
    Il semble y avoir un problème avec le MCP7940N le registre PWRDNMTH ne lit que 0 ou 255.

2018-02-09
----------
1. corrigé problème dans alarm_msg, ring_tone n'était pas terminé par {0,0}.
2. ajouter sound_init() dans module sound.
3. désactivation interruption sur alarme lorsqu'aucune alarme n'est active.

2018-02-08
----------
1. Travail sur interruption alarmes.  Problème à résoudre avec le son qui n'arrête
pas correctement à la find du ring_tone dans la routine alarm_msg(). Priorité d'interruption?

2018-02-06
----------
1. Travail sur rtc.c fonctions d'alarmes.
2. ajout commande alarme dans shell.c

2018-02-05
----------
1. Travail sur rtcc.c, correction erreur interface I2C.

2018-02-04
----------
1. Travail dans rtcc.c

2018-02-03
-----------
1. Modification schématique ajout filtre sur alimentation du MCP7940N.
2. Travail sur interface I2C. Interface maintenant fonctionnelle.


2018-02-02
----------
1.  Correction erreur schématique.
2.  Travail sur rtcc.c interface I2C

2018-02-01
----------
1. travail sur rtcc.c, interface I2C

2018-01-31
----------
1. Modification schématique. Déplacement des signaux SCL de l'interface I2C sur
broche RB0 (pin 4) et déplacement cathode power LED sur BP14 (pin 25)
2. travail sur interface i2c pour rtcc.

2018-01-30
----------
1. Travail sur RTCC.c

2018-01-29
----------
1. Modification schématique. Ajout RTCC MCP2940N
2. Création du pilote pour le RTCC.

2018-01-27
----------
modifié schématique pour que la cathode de la power LED soit branchée  à la broche
RB0 (pin 4) du MCU. La LED peut maintenant être contrôlée par le MCU.
1. Lorsque l'initialisation est complétée sans erreur la LED est allumée.
2. Lorsqu'il se produit une exception la LED clignote.

Commencé à travaillé sur les service call.


2018-01-25,26
-------------
expérimentation avec syscall, rien de concluant.

2018-01-23
----------
modification à keyboard.c et edit.c
modification au projet ps2_rs232 pour ajouter touches virtuelles &lt;CTRL&gt;-&lt;DEL&gt; 
et changer la valeur de &lt;CTRL&gt;-&lt;BACKSPACE&gt;
Dans vm.h remplacé les '#define' des opcodes par une énumération.
Retiré vpcBasic du projet.
Modification à la schématique pour ajouter l'ampli audio LM386.

2018-01-21
----------
Modification du fonctionnement du curseur texte de la console locale. Maintenant la minuterie est
assurée par l'interruption du TIMER2.

ajouts des commandes uptime, et date dans shell.c
modification dans hardware.c pour ajouter date.

