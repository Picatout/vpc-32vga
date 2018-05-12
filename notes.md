2018-05-11
----------
bogue dans spread save/load sequence �choue.


2018-05-10
----------
1. Travail sur les nombres � virgules flotttantes.
2. Corrig� bogue dans factor().
3. Corrig� bogue dans VM:negfloat
4. Corrig� inversion de l'orde de float_to_int et int_to_float dans opcodes_table.
5. Modifi� le fonctionnement des expression(). Maintenant les expressions retourne
   type eVAR_INT ou eVAR_FLOAT. �limination de la variable globale **expr_type**.
6. Corrig� bogue dans var_type_from_name(). break oubli� dans case '!'.

2018-05-09
----------
1. Modifi� BASIC:putc pour accepter les sorties sur fichier.
2. Corrig� bogue dans fileio:set_filter
3. Modifi� BASIC:print pour accepter les sorties sur fichier.
4. Suppression des commandes  BASIC superflues FGETS et FPUTS.
5. Modification de BASIC:input pour lire donn�es � partir d'un fichier.
6. Corrig� bogue dans editor:line_break()
� faire: ajouter support pour nombre virgule flottante.

2018-05-08
----------
1. Ajout des fonctions d'acc�s � des fichiers texte. OPEN,CLOSE,EOF,SEEK,FPUTC,FPUTS
2. Corrig� bogue dans VM.type
3. Corrig� bogue dans VM.str_len
4. Corrig� bogue dans VM.mid

2018-05-07
----------
1.  Remplac� eSTOP par eNL plus explicite.
2.  Modification � string_expression() pour ignorer les eNL
3.  Modification � expression() pour ignorer les eNL
4.  ex�cuter les programmes basic de test.
Probl�me � r�soudre: commande free tourne en boucle infinie parfois.

2018-05-05
----------
1. retour au commit du 3 mai, 21c827614e5d8a68038f2c83a0d0bccf51c51549
2. Modifification dans kw_local() poura accepter les variables de type byte.
3. Ajout de dp0  dans la VM.
4. Modification � to_hex et to_int dans VM.
5. Modification de l'ordre des arguments dans kw_hex() de BASIC.
6. Correction d'un bogue dans BASIC:skip_space(), parenth�ses dans l'expression
   du while �taient incorrecte.
7. Ajout de l'op�rateur IDP0 dans la VM. Lorsque bye est invoqu� � l'int�rieur d'une 
   structure de contr�le, IDP0 est compil� avant IBYE pour s'assurer que la pile
   dstack est vide � la sortie de la VM.


2018-05-03
----------
1. d�bogu� spred.bas, bogue dans input lib�rais le pad. 
2. Ajout de PEEK() et HEX$()
a faire: corrig� bogue dans spred.bas lorsque save plusieurs fois plante.

2018-05-02
----------
1. Test BASIC
2. Correction bogue dans subrtn_create()
3. Correction bogue dans try_string_compare()
4. Correcton bogue dans kw_dim()
5. Correction bogue dans dim_array()
6. Correction bogue dans next_token(),r�ception de '\n' tiens compte de cmplevel.

2018-05-01
----------
1. Travail sur la nouvelle gestion des cha�nes dans BASIC. Avec ce nouveau
   syst�me toutes les cha�nes sont allou�e dans l'espace RAM dynamique (heap).
2. Modification du type de la variable **abort_signal** de bool � uint8_t.  

2018-04-30
----------
1. Travail sur la nouvelle gestion des cha�nes dans BASIC. Avec ce nouveau
   syst�me toutes les cha�nes sont allou�e dans l'espace RAM dynamique (heap).
   

2018-04-29
----------
1. Ajout des fonction cha�nes **UCASE$** et **LCASE$**
2. entrepris d'apporter des modifications majeures � la gestion des cha�nes
   en cr�ant un typedef avec une strucure **dstring_t** pour enregistrer
   les r�f�rences aux cha�nes dynamques. Cette m�thode va simplifier la gestion
   des cha�nes dynamiques. Cr�ation de la branche dstring.

2018-04-28
----------
1. Corrig� string_expression()
2. Ajouter comparison de cha�ne, i.e. cha�ne1 op_rel cha�ne2, retourne -1,0,1
   fait appel � strcmp() de C.
3. Ajout du typage des variables avec le mot r�serv� **AS**
   types support�s: INTEGER,FLOAT,BYTE,STRING
4. Corrig� bogue r�gressif dans kw_print()
5. Modification � kw_dim().


2018-04-27
----------
1. BASIC, Modification � string_expression()
2. Hardware_Profile, Modification a biggest_chunck() et et free_heap().
3. BASIC, coulage m�moire heap � corriger.
4. bogue dans free_heap()
5. bogue dans string_expression() � corriger.

2018-04-26
----------
1. Test� et d�bogu� commande BASIC PLAY().
2. Ajout de la concatenation des cha�nes par op�rateur '+'
3. Corrig� bogue dans RIGHT$
4. bogue � corriger le compilateur ne lib�re pas toutes les cha�ne lors de la
   compilation des expressions cha�nes concat�n�es.

2018-04-25
----------
1. Travail sur commande BASIC PLAY().


2018-04-24
----------
1. Travail sur commande BASIC PLAY().
2. Nombreuses modifications dans le module sound.c

2018-04-23
----------
1. Travail sur sound.c, un 3�eme argument indique la portion de la dur�e
   qui est jou�e par rapport � la dur�e totale. L'�num�ration TONE_FRACTION a
   �t� cr�er � cet effet.
2. Ajout de **void beep()**.
3. Ajout de **void sound(unsigned int frq, unsigned duration)**
4. D�but� travail sur commande BASIC PLAY().


2018-04-22
----------
1. Modification de dict_entry_t pour s�parer l'entier uint8_t len en 2 champs,
   len et fntype. Cr�ation de l'�num�ration FN_TYPE. Modification du code pour
   l'utilisation de cette nouvelle structure.
2. Modification de la structure token_t et du code pour tenir compte de la nouvelle
   structure.
3. Corrig� parse_arg_list() qui ne tenait pas compte des fonctions retournant une cha�ne.

2018-04-21
----------
1. d�bogu� INSERT$
2. Modification HardwareProfile.c:CoreTimerHandler()
3. D�bogu� SUBST$
4. Ajout de INSTR
5. Modificication de dict_entry_t, utilisation � revoir.

2018-04-20
----------
1. Modifi� graphics:sprite() pour que les sprites soient compatibles avec ceux utilis�s
   dans le projet PV16SOG.
2. Ajout� un m�canisme permettant � l'utilisateur d'interrompre un programme BASIC par 
   la combinaison de touches CTRL_C.
3. Ajout� les commandes DATE$ et TIME$ dans BASIC.
4. Ajout� les fonctions LEFT$,RIGHT$,MID$,APPEND$,PREPEND$ et SUBT$
� faire: corrig� bogue dans INSERT$

2018-04-19
----------
1. Corrig� bogue r�gressif dans parse_arg_list().
2. Renomm� SETPIXEL en PSET conform�ment � QBASIC.
3. Ajout des commandes BASIC  ASC() et CHR$(). 
4. Corrig� bogue r�gressif, variable locale ne pouvait avoir le m�me nom qu'une
   variable globale.

2018-04-18
----------
1. Ajout de la directive DECLARE dans vpcBASIC.
2. Renomm� PAUSE en SLEEP,  TONE en SOUND comme dans QBASIC
3. Ajout de STR$() et VAL() pour conversion entre cha�ne et num�rique (i.e. QBASIC).
4. Modification de arg_parse_list().
5. Modification � var_t pour ajouter le champ **local**
a faire: revoie la fa�on dont le compilateur traite les variables locales.
a faire: Ajout du support pour les nombres � virgules flotantes dans vpcBASIC.

2018-04-17
----------
1. compl�ter modification � la commande shell:dir.
2. apport� des modifications a fileio en d�placant les fonctions filtre dans ce module.

2018-04-16
----------
1. Corrig� bogue dans **BASIC_shell()**. Modifier **kw_bye()**. Rendue la variable **vm:f_trace**
   globale pour l'acc�der directement dans BASIC_shell().
2. Extrait de fileio:listDir() l'impression de Finfo pour en faire une proc�dure export�e. print_fileinfo().
3. Modification de shell:cmd_dir() pour accepter le caract�re wildcard dans les noms de fihiers.

2018-04-15
----------
1. Modification de vm:mslash, ne requi�re plus math:mdiv(), le module math a �t� supprim� du projet.
2. Modification de basic:TONE pour ajouter un 3i�me param�tre d'attente. TONE(freq,dur�e,attend)
3. Corrig� bogue dans BASIS_shell(), pour les options EXEC_STRING et EXEC_STAY, setjmp() n'�tait
   pas initialis�, causant un crash de l'ordinateur en cas d'appel throw().
4. Corrig� bogue dans graphics:box(), arguments height et widht �taient invers�s dans
   le contr�le de limite des boucles for().
5. Corrig� bogue dans basic:compile(). Le code compilant les appels de sous-routine
   �tait incorrect.

2018-04-14
----------
1. Corrig� bogue dans vt_clear_screen()
2. Correction bogues dans editor.
3. Remplac� editor:ask_confirm() par ask_save_changes().
4. Correction bogue vm:cfeth, remplac� instruction **lb** par **lbu**.
5. Correction bogue vm:fortest
6. Modification de l'utlisation de var_t, champ **dim** utilis� pour le nombre d'arguments
   Modification de create_arg_list(), factor() et compile() pour tenir compte
   de ce nouvel usage.
7. Corrig� bogue dans basic:BASIC_shell().


2018-04-13
----------
1. Test BASIC.
2. corrig� bogue dans vm:xor_pixel
3. Modification � vm:rectangle() et vm:box()
4. Modification � graphics:polygon()
5. Corrig� bogue r�gressif dans vm:print_state()
6. Corrig� bogue r�gressif dans basic:factor()
7. Corrig� bogue dans array_code_address()
 
2018-04-12
----------
1. Test BASIC.
2. Ajouter code dans vm.S pour contr�ler le d�bordement des piles dstack et rstack
3. � faire: trouver un algorithme pour fill() moins r�cursif.

2018-04-11
----------
1. Test BASIC.
2. Ajout du module **math.c** et de la fonction **mdiv()**.
3. Corrig� bogue dans **kw_wend()**.

2018-04-10
----------
1. Modification � la commande cmd_basic() dans shell.c pour accommoder l'option **-c "BASIC code"** et
   **-k "BASIC code"**.
2. Modification de l'interface de BASIC_shell() pour accommoder le passage d'option.
3. Ajout de VGACLS dans BASIC.
4. Modifi� circle() pour correcger l'aspect qui du cercle qui �tait en fait elliptique.
5. Correction boguqe dans LEN().

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

