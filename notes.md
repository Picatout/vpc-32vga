2018-04-09
----------
1. Corrig� bogue dans code_let_string().
2. tester BASIC.
3. Corriger bogue dans INPUT
4. Corriger bogue dans fonction bool�enne.
5. Ajout fonctiopns bool�enne bool_not,bool_or,bool_and  dans VM.

2018-04-08
----------
1. Travail sur vpcBASIC.
2. Correction code_let_string().
3. Modification � la gestion des erreurs du compilateur et de la VM.
4. Retravailler la boucle read-compile-execute

2018-04-07
----------
1. Travail sur vpcBASIC.
2. Changement au saut relatif. Au d�part je pensait que codage des saut relatif
   sur 1 octet {-128...127} serait suffisant. En pratique il s'est av�r� insuffisant
   je vais donc l'�trendre � 2 octets {-32768..32767}. Il faut donc modifi� la machine
   virtuelle et le compilateur en cons�quence. 
3. Modification � la **VM** pour que trace indique le nom symbolique au lieu du opcode.
   la macro _dict ajout le nom symbolique avant chaque opcode de la machine virtuelle
   et **print_state** affiche ce nom.

2018-04-06
----------
1. Travail sur vpcBASIC.
2. D�boguer m�canisme d'appel des fonctions et sous-routines. 

2018-04-05
----------
1. Travail sur vpcBASIC.
2. Modifications pour tenir compte de la nouvelle d�finition de la structure **var_t**
   Les champes **ro**, **array** et **dim** sont maintenant utilis�s. L'�num�ration **var_type_e**
   a �t� modifi�e en cons�quence.
3. bogue � corrig�e: ? 4*f(n) imprime 0.


2018-04-04
----------
1. Travail sur vpcBASIC. Ajout des fonctions d'acc�s � la m�moire SPI RAM.
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
5. Ajout de la fonction insert_line() � la console.
6. Nettoyage vms

2018-03-31
----------
1. Travail sur vpcBASIC.
2. Modification du type **var_t** pour besoins futur.
3. ajout eVAR_CONST_STR dans dans l'�num�ration var_type_e
4. Modifications dans kw_const() et kw_let() pour tenir compte de eVAR_CONST_STR
5. Ajout�  **curline** et **curpos** dans vm.S ainsi que macro **_console**


2018-03-30
----------
1. Travail sur vpcBASIC
2. Travail bogue commande INPUT. INPUT maintenant fonctionnel
3. modification � allocation des cha�nes, maintenant allou�e dyanmiquement sur le heap.
4. correcion bogue fonction MIN et MAX.
5: tests de vitesse:
    boucle FOR ... NEXT vide: 100 000 boucles prend 521msec.
    boucle WHILE ... WEND avec incr�ment de comtpeur sur 1 seconde: 112366 it�rations 

2018-03-29
----------
1. Travail sur vpcBASIC et vm.S
2. Travail sur INPUT

2018-03-28
----------
1. Travail sur vpcBASIC et vm.S
2. Probl�me avec SUB et FUNC r�solue.
3. Modification impression cha�ne lit�rale. Remplacement de cha�ne compt�e par ASCIIZ.
   Renomm� dotq par prt_str dans vm.S.
4. Travail sur INPUT


2018-03-27
----------
1. Travail sur vm.S, ajout de dots, depth et modfication de next, set_frame, leave
2. Travail sur vpcBASIC modification compilation concernant SUB et FUNC.
3. ***** A r�soudre  probl�me de la valeur de retour des FUNC.

2018-03-26
----------
1. travail sur machine virtuelle. Changer le fonctionnement des piles. Elles �taient
pr�-inc�ment�es, maintenant elles sont post-incr�ment�es.
2. Suppression des mots suivants dans la vm: wlit,docol,exit,exec,umsmod,msmod
2018-03-24
----------
1. modification � la machine virtuelle vm.
2. travail sur kw_func et kw_sub 

2018-03-24
----------
1. Travail sur vpcBASIC. 
2. renomm� fix_branch_address() en fix_fore_jump()
3. cr�er fix_back_jump()
4. Travail sur SELECT CASE
5. Modifi� code VM pour IBRA

2018-03-23
----------
1. Travail sur interpr�teur BASIC et la machine virtuelle. Les contr�le de flux.
2. Ajout de vt_read_line(), modification de read_line(), ser_read_line() et kbd_read_line()
3. Travail sur structure FOR...NEXT

2018-03-22
----------
1. Travail sur interpr�teur BASIC et la machine virtuelle.


2018-03-21
----------
1. Travail sur interpr�teur BASIC et la machine virtuelle.


2018-03-17
----------
1. Travail sur interpr�teur BASIC et la machine virtuelle.


2018-03-17
----------
1. Travail sur interpr�teur BASIC et la machine virtuelle.
2. diskio:get_fattime()  ajout du code pour VPC_32VGA
3. Correction bogue dans console:clear_line()
4. Correction bogue dans vga:vga_clear_line()

2018-03-16
----------
1. Travail sur serial_comm.c, serial_rx_isr(void):r�duction limite avant d'envoyer XOFF.
2. Modification de console:clear_line().
3. Impl�mentation de vga_clear_line() et modification de vt_clear_line()
4. Commencer travail sur machine virtuelle � piles vms.S

2018-03-15
----------
1. Travail sur serial_comm.c
2. Modification au projet ps2_rs232 pour ajouter touche virtuelle VK_SDEL=191
3. Modification � vt100.c et keyboard.h pour ajouter touche VK_SDEL.
4. Modification� l'�diteur pour remplac� VK_CBACK par VK_SDEL comme touche effacement d�but ligne. 

2018-03-14
----------
1. Travail sur serial_comm.c, ajout interruption et tampon de r�ception. 
2. Modification keyboard:kbd_rx_isr() pour ajouter gestion d'erreur sur r�ception.


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
2. ajouter  kw_run, modification pour support� la commande RUN.


2018-03-07
----------
1. Travail sur editor.c
2. ajout de console:set_tab_width(),vt100:vt_set_tab_width() et vga:vga_set_tab_widh()
3. Suppression de console:set_auto_scroll(),vt100:vt_set_auto_scroll()
4. Corrig� erreur dans vga_put_char() pour le traitement du caract�re '\t'.

2018-03-06
----------
1. Travail sur editor.c

2018-03-03
----------
1. Travail sur �diteur de texte.
2. Ajout fonction editor:invert_display()

2018-03-02
----------
1. Retraivaill� l'�diteur de texte. 
2. Corrig�e bogue dans HardwareProfile:free_heap();
3. Ajout de la fonction HardwareProfile:biggest_chunk() qui retourne le plus gros
   morceau de m�moire RAM allouable par malloc().

2018-02-28
----------
1. Cr�ation branche new_editor pour travail dans editor.c


2018-02-27
----------
1. Travail sur shell. Corrig� bogue et ajout� commande 'con'.
2. Modification � ser_read_line().

2018-02-26
----------
1. Travail sur shell. bogue sur variable nom longueur 1.

2018-02-25
----------
1. Travail sur new_shell.

2018-02-24
-----------
1. Cr�ation de la branche new_shell.
2. Refonte majeure de shell.c Toute commande est une foncion qui retourne une
   cha�ne de caract�re. Le shell imprime la cha�ne retourn�e par la commande et
   lib�re la m�moire r�serv�e pour cette ch�ine.

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
1. Uniformisation des noms de fonctions d'interface p�riph�riques

2018-02-18
-----------
1.  Transfert des fonctions lecture clavier de vga.c vers keyboard.c

2018-02-17
----------
1. transf�rer les fonctions reli� � l'affichage vid�o vga de console.c vers vga.c

2018-02-16
----------
1. R�organisation du projet. Pour le moment on enl�ve du projet les modules shell,
    console, vm, editor, graphics, vt100, pour se concentrer sur le coeur du syst�me
    d'exploitation. Il faut developper un API.

2018-02-13
----------
1. travail sur console pour cr�er une interface commune entre console locale et rs-232.


2018-02-12
----------
1. �valuation du temps CPU pris par les interruptions.

2018-02-11
----------
1. Ajout commande **clktrim** dans le shell.
2. Ajout commande **echo** dans shell.
3. Ajout function **spaces()** dans module console. 

2018-02-10
-----------
1. Modification � print_hex() et print_int();
2. Ajout message au d�marrage du shell pour indiquer la date et heure dernier power down.
    Il semble y avoir un probl�me avec le MCP7940N le registre PWRDNMTH ne lit que 0 ou 255.

2018-02-09
----------
1. corrig� probl�me dans alarm_msg, ring_tone n'�tait pas termin� par {0,0}.
2. ajouter sound_init() dans module sound.
3. d�sactivation interruption sur alarme lorsqu'aucune alarme n'est active.

2018-02-08
----------
1. Travail sur interruption alarmes.  Probl�me � r�soudre avec le son qui n'arr�te
pas correctement � la find du ring_tone dans la routine alarm_msg(). Priorit� d'interruption?

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
1. Modification sch�matique ajout filtre sur alimentation du MCP7940N.
2. Travail sur interface I2C. Interface maintenant fonctionnelle.


2018-02-02
----------
1.  Correction erreur sch�matique.
2.  Travail sur rtcc.c interface I2C

2018-02-01
----------
1. travail sur rtcc.c, interface I2C

2018-01-31
----------
1. Modification sch�matique. D�placement des signaux SCL de l'interface I2C sur
broche RB0 (pin 4) et d�placement cathode power LED sur BP14 (pin 25)
2. travail sur interface i2c pour rtcc.

2018-01-30
----------
1. Travail sur RTCC.c

2018-01-29
----------
1. Modification sch�matique. Ajout RTCC MCP2940N
2. Cr�ation du pilote pour le RTCC.

2018-01-27
----------
modifi� sch�matique pour que la cathode de la power LED soit branch�e  � la broche
RB0 (pin 4) du MCU. La LED peut maintenant �tre contr�l�e par le MCU.
1. Lorsque l'initialisation est compl�t�e sans erreur la LED est allum�e.
2. Lorsqu'il se produit une exception la LED clignote.

Commenc� � travaill� sur les service call.


2018-01-25,26
-------------
exp�rimentation avec syscall, rien de concluant.

2018-01-23
----------
modification � keyboard.c et edit.c
modification au projet ps2_rs232 pour ajouter touches virtuelles &lt;CTRL&gt;-&lt;DEL&gt; 
et changer la valeur de &lt;CTRL&gt;-&lt;BACKSPACE&gt;
Dans vm.h remplac� les '#define' des opcodes par une �num�ration.
Retir� vpcBasic du projet.
Modification � la sch�matique pour ajouter l'ampli audio LM386.

2018-01-21
----------
Modification du fonctionnement du curseur texte de la console locale. Maintenant la minuterie est
assur�e par l'interruption du TIMER2.

ajouts des commandes uptime, et date dans shell.c
modification dans hardware.c pour ajouter date.

