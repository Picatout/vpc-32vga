2018-05-11
----------
bogue dans spread save/load sequence échoue.


2018-05-10
----------
1. Travail sur les nombres à virgules flotttantes.
2. Corrigé bogue dans factor().
3. Corrigé bogue dans VM:negfloat
4. Corrigé inversion de l'orde de float_to_int et int_to_float dans opcodes_table.
5. Modifié le fonctionnement des expression(). Maintenant les expressions retourne
   type eVAR_INT ou eVAR_FLOAT. Élimination de la variable globale **expr_type**.
6. Corrigé bogue dans var_type_from_name(). break oublié dans case '!'.

2018-05-09
----------
1. Modifié BASIC:putc pour accepter les sorties sur fichier.
2. Corrigé bogue dans fileio:set_filter
3. Modifié BASIC:print pour accepter les sorties sur fichier.
4. Suppression des commandes  BASIC superflues FGETS et FPUTS.
5. Modification de BASIC:input pour lire données à partir d'un fichier.
6. Corrigé bogue dans editor:line_break()
À faire: ajouter support pour nombre virgule flottante.

2018-05-08
----------
1. Ajout des fonctions d'accès à des fichiers texte. OPEN,CLOSE,EOF,SEEK,FPUTC,FPUTS
2. Corrigé bogue dans VM.type
3. Corrigé bogue dans VM.str_len
4. Corrigé bogue dans VM.mid

2018-05-07
----------
1.  Remplacé eSTOP par eNL plus explicite.
2.  Modification à string_expression() pour ignorer les eNL
3.  Modification à expression() pour ignorer les eNL
4.  exécuter les programmes basic de test.
Problème à résoudre: commande free tourne en boucle infinie parfois.

2018-05-05
----------
1. retour au commit du 3 mai, 21c827614e5d8a68038f2c83a0d0bccf51c51549
2. Modifification dans kw_local() poura accepter les variables de type byte.
3. Ajout de dp0  dans la VM.
4. Modification à to_hex et to_int dans VM.
5. Modification de l'ordre des arguments dans kw_hex() de BASIC.
6. Correction d'un bogue dans BASIC:skip_space(), parenthèses dans l'expression
   du while étaient incorrecte.
7. Ajout de l'opérateur IDP0 dans la VM. Lorsque bye est invoqué à l'intérieur d'une 
   structure de contrôle, IDP0 est compilé avant IBYE pour s'assurer que la pile
   dstack est vide à la sortie de la VM.


2018-05-03
----------
1. débogué spred.bas, bogue dans input libérais le pad. 
2. Ajout de PEEK() et HEX$()
a faire: corrigé bogue dans spred.bas lorsque save plusieurs fois plante.

2018-05-02
----------
1. Test BASIC
2. Correction bogue dans subrtn_create()
3. Correction bogue dans try_string_compare()
4. Correcton bogue dans kw_dim()
5. Correction bogue dans dim_array()
6. Correction bogue dans next_token(),réception de '\n' tiens compte de cmplevel.

2018-05-01
----------
1. Travail sur la nouvelle gestion des chaînes dans BASIC. Avec ce nouveau
   système toutes les chaînes sont allouée dans l'espace RAM dynamique (heap).
2. Modification du type de la variable **abort_signal** de bool à uint8_t.  

2018-04-30
----------
1. Travail sur la nouvelle gestion des chaînes dans BASIC. Avec ce nouveau
   système toutes les chaînes sont allouée dans l'espace RAM dynamique (heap).
   

2018-04-29
----------
1. Ajout des fonction chaînes **UCASE$** et **LCASE$**
2. entrepris d'apporter des modifications majeures à la gestion des chaînes
   en créant un typedef avec une strucure **dstring_t** pour enregistrer
   les références aux chaînes dynamques. Cette méthode va simplifier la gestion
   des chaînes dynamiques. Création de la branche dstring.

2018-04-28
----------
1. Corrigé string_expression()
2. Ajouter comparison de chaîne, i.e. chaîne1 op_rel chaîne2, retourne -1,0,1
   fait appel à strcmp() de C.
3. Ajout du typage des variables avec le mot réservé **AS**
   types supportés: INTEGER,FLOAT,BYTE,STRING
4. Corrigé bogue régressif dans kw_print()
5. Modification à kw_dim().


2018-04-27
----------
1. BASIC, Modification à string_expression()
2. Hardware_Profile, Modification a biggest_chunck() et et free_heap().
3. BASIC, coulage mémoire heap à corriger.
4. bogue dans free_heap()
5. bogue dans string_expression() à corriger.

2018-04-26
----------
1. Testé et débogué commande BASIC PLAY().
2. Ajout de la concatenation des chaînes par opérateur '+'
3. Corrigé bogue dans RIGHT$
4. bogue à corriger le compilateur ne libère pas toutes les chaîne lors de la
   compilation des expressions chaînes concaténées.

2018-04-25
----------
1. Travail sur commande BASIC PLAY().


2018-04-24
----------
1. Travail sur commande BASIC PLAY().
2. Nombreuses modifications dans le module sound.c

2018-04-23
----------
1. Travail sur sound.c, un 3ìeme argument indique la portion de la durée
   qui est jouée par rapport à la durée totale. L'énumération TONE_FRACTION a
   été créer à cet effet.
2. Ajout de **void beep()**.
3. Ajout de **void sound(unsigned int frq, unsigned duration)**
4. Débuté travail sur commande BASIC PLAY().


2018-04-22
----------
1. Modification de dict_entry_t pour séparer l'entier uint8_t len en 2 champs,
   len et fntype. Création de l'énumération FN_TYPE. Modification du code pour
   l'utilisation de cette nouvelle structure.
2. Modification de la structure token_t et du code pour tenir compte de la nouvelle
   structure.
3. Corrigé parse_arg_list() qui ne tenait pas compte des fonctions retournant une chaîne.

2018-04-21
----------
1. débogué INSERT$
2. Modification HardwareProfile.c:CoreTimerHandler()
3. Débogué SUBST$
4. Ajout de INSTR
5. Modificication de dict_entry_t, utilisation à revoir.

2018-04-20
----------
1. Modifié graphics:sprite() pour que les sprites soient compatibles avec ceux utilisés
   dans le projet PV16SOG.
2. Ajouté un mécanisme permettant à l'utilisateur d'interrompre un programme BASIC par 
   la combinaison de touches CTRL_C.
3. Ajouté les commandes DATE$ et TIME$ dans BASIC.
4. Ajouté les fonctions LEFT$,RIGHT$,MID$,APPEND$,PREPEND$ et SUBT$
à faire: corrigé bogue dans INSERT$

2018-04-19
----------
1. Corrigé bogue régressif dans parse_arg_list().
2. Renommé SETPIXEL en PSET conformément à QBASIC.
3. Ajout des commandes BASIC  ASC() et CHR$(). 
4. Corrigé bogue régressif, variable locale ne pouvait avoir le même nom qu'une
   variable globale.

2018-04-18
----------
1. Ajout de la directive DECLARE dans vpcBASIC.
2. Renommé PAUSE en SLEEP,  TONE en SOUND comme dans QBASIC
3. Ajout de STR$() et VAL() pour conversion entre chaîne et numérique (i.e. QBASIC).
4. Modification de arg_parse_list().
5. Modification à var_t pour ajouter le champ **local**
a faire: revoie la façon dont le compilateur traite les variables locales.
a faire: Ajout du support pour les nombres à virgules flotantes dans vpcBASIC.

2018-04-17
----------
1. compléter modification à la commande shell:dir.
2. apporté des modifications a fileio en déplacant les fonctions filtre dans ce module.

2018-04-16
----------
1. Corrigé bogue dans **BASIC_shell()**. Modifier **kw_bye()**. Rendue la variable **vm:f_trace**
   globale pour l'accéder directement dans BASIC_shell().
2. Extrait de fileio:listDir() l'impression de Finfo pour en faire une procédure exportée. print_fileinfo().
3. Modification de shell:cmd_dir() pour accepter le caractère wildcard dans les noms de fihiers.

2018-04-15
----------
1. Modification de vm:mslash, ne requière plus math:mdiv(), le module math a été supprimé du projet.
2. Modification de basic:TONE pour ajouter un 3ième paramètre d'attente. TONE(freq,durée,attend)
3. Corrigé bogue dans BASIS_shell(), pour les options EXEC_STRING et EXEC_STAY, setjmp() n'était
   pas initialisé, causant un crash de l'ordinateur en cas d'appel throw().
4. Corrigé bogue dans graphics:box(), arguments height et widht étaient inversés dans
   le contrôle de limite des boucles for().
5. Corrigé bogue dans basic:compile(). Le code compilant les appels de sous-routine
   était incorrect.

2018-04-14
----------
1. Corrigé bogue dans vt_clear_screen()
2. Correction bogues dans editor.
3. Remplacé editor:ask_confirm() par ask_save_changes().
4. Correction bogue vm:cfeth, remplacé instruction **lb** par **lbu**.
5. Correction bogue vm:fortest
6. Modification de l'utlisation de var_t, champ **dim** utilisé pour le nombre d'arguments
   Modification de create_arg_list(), factor() et compile() pour tenir compte
   de ce nouvel usage.
7. Corrigé bogue dans basic:BASIC_shell().


2018-04-13
----------
1. Test BASIC.
2. corrigé bogue dans vm:xor_pixel
3. Modification à vm:rectangle() et vm:box()
4. Modification à graphics:polygon()
5. Corrigé bogue régressif dans vm:print_state()
6. Corrigé bogue régressif dans basic:factor()
7. Corrigé bogue dans array_code_address()
 
2018-04-12
----------
1. Test BASIC.
2. Ajouter code dans vm.S pour contrôler le débordement des piles dstack et rstack
3. À faire: trouver un algorithme pour fill() moins récursif.

2018-04-11
----------
1. Test BASIC.
2. Ajout du module **math.c** et de la fonction **mdiv()**.
3. Corrigé bogue dans **kw_wend()**.

2018-04-10
----------
1. Modification à la commande cmd_basic() dans shell.c pour accommoder l'option **-c "BASIC code"** et
   **-k "BASIC code"**.
2. Modification de l'interface de BASIC_shell() pour accommoder le passage d'option.
3. Ajout de VGACLS dans BASIC.
4. Modifié circle() pour correcger l'aspect qui du cercle qui était en fait elliptique.
5. Correction boguqe dans LEN().

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

