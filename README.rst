**QTImage2Mov ([*****QT*** ***I*****mage *****To*** ***M*****ovie])
-
version 1.20**

::QTImage2Mov is an importer component for XML containers listing import
commands, as well as
    for a very simple image stream container format (JPGS) and a
    proprietary video format (VOD).

    The XML container format allows to specify one or multiple import
    files as well as to configure the import procedure (e.g. mask quadrants
    or flip) and add chapters or other metadata.
    Note that this repository does NOT contain the VOD import code which
    cannot at this time be made public.



QTImage2Mov est un plug-in pour le "noyau audio-visuel" QuickTime? ("Importer
Component" dans le jargon d'Apple) qui permet d'importer certains types de
fichiers ? notamment des fichiers .VOD enregistr?s sur les enregistreurs
Brigade ? et d'en faire des vid?os QuickTime. Cette approche a l'int?r?t que,
sans effort suppl?mentaire, les fichiers .VOD ainsi que les vid?os
r?sultantes s'ouvrent avec le lecteur QuickTime ou tout autre application
faisant appel ? QuickTime, mise ? part du fait que QuickTime fournit
gratuitement une multitude de fonctionnalit?s multim?dia.

Nouveaut? de la version 1.20 : l'importation des fichiers VOD contenant du
MPEG4.

L'importation donne lieu ? un objet en m?moire vive (un *Movie*) qui
repr?sente et prend la forme d'une vid?o sur l'?cran, dans laquelle sont
affich?es les trames du fichier import? (un fichier .VOD par exemple). On
peut ouvrir directement un fichier .VOD. Dans ce cas, le fichier s'ouvre avec
un jeu de r?glages par d?faut (d?crits ci-dessous). Si ces r?glages ne
conviennent pas, par exemple s'il faut ouvrir seulement le flux d'une des 4
cam?ras enregistr?es dans un fichier .VOD, il faut passer par un fichier
.QI2M (pour QtImage2Mov). Ce fichier est un document XML qui pr?cise quel
fichier(s) importer, et comment. Cette "interface de r?glage" a ?t? choisi
pour sa facilit? (pas besoin de concevoir une interface graphique; cr?ation
de fa?on programmatique, ...) et son "universalit?". Sa forme basique peut
ressembler ? ceci:





<?xml version="1.0"?> <?quicktime type="video/x-qt-img2mov"?> <!--
commentaire --> <import askSave=False autoSave=False > <description
txt="ligne 1 (sans accents!)" lang="fr_FR" /> <chapter src="titre du
chapitre" /> <sequence src="tstVOD.VOD" freq=-1 interval=-1 maxframes=-1
channel=-1 starttime=True
 hidetc=False hidets=False timepad=False transH=0 transV=0 relTransH=0
 relTransV=0 hflip=False
 vmgi=True log=False description="une description"
 asmovie=False newchapter=True /> </import>





Ici, on importe un seul fichier tstVOD.VOD avec les r?glages par d?faut (voir
ci-dessous) dans une piste d?di?e d'une nouvelle vid?o QuickTime. La piste et
la vid?o enti?re re?oivent chacune une description individuelle. Le 'movie'
contiendra une piste *TimeCode* qui sera visible et affichera une indication
du temps d'enregistrement de chaque trame dans le format
heure:min:sec;trames. En plus, les horodatages de chaque trame (image) ainsi
que des donn?es GPS sont stock?s dans une piste texte (*timeStamp*) quand la
source contient ces informations (ce qui est le cas pour les fichiers VOD).
Si les informations GPS existent, le texte est en gras si les donn?es sont
valides (bonne r?ception GPS), sinon en 'normal' (dans ce cas les valeurs ne
sont pas forc?ment mises ? jour jusqu'? ce le GPS captait ? nouveau
correctement). Cette piste est invisible par d?faut (cf. le r?glage
d'importation **channel**, ci-dessous).

Les descriptions ainsi que certaines informations pertinentes concernant le
fichier source (tstVOD.VOD) sont stock?es dans les meta-donn?es de la vid?o
et consultables via l'API QuickTime.





Il est important de noter que le comportement par d?faut est celui d'un
lecteur, sans effets secondaires: la vid?o est ouverte (import?e), mais rien
n'est modifi? ou sauvegard? sur disque.





Structure du fichier xml .QI2M





Le fichier .QI2M comporte une section principale, ***import***, qui regroupe
la ou les commandes d'importation qui donneront lieu aux donn?es audio-
visuelles de la vid?o. Les commandes principales sont d?crites ci-dessous.





Commandes principales dans la section ***import***





La commande ***description*** ajoute des informations textuelles ? la vid?o;
le texte sp?cifi? par chaque commande est ajout? ? la description d?j?
existante. On peut ?ventuellement pr?ciser le langage utilis?, mais on ne
peut pas utiliser des caract?res avec accents.





La commande ***chapter*** demande la cr?ation d'un marqueur de chapitre au
temps actuel dans la vid?o. Le contr?leur de QuickTime affiche ces marqueurs
dans un menu 'popup', permettant une navigation rapide dans une vid?o ?
plusieurs pistes cons?cutives. Cette commande accepte deux param?tres
optionnels: *starttime=t* permet de sp?cifier le temps relatif au d?but de la
vid?o (en secondes) o? le marqueur doit ?tre ins?r? et *duration=d* permet de
pr?ciser la dur?e. Attention si un titre est ins?r? ? un temps d?j? existant
dans la piste (au lieu d'?tre ajout? ? la fin de la piste). Dans ce cas, le
segment correspondant ? *starttime* et *duration* est supprim? de la piste
qui contient les marqueurs de chapitre pour ?viter un d?calage d'?ventuels
marqueurs suivants (et il est donc possible de supprimer un marqueur existant
bien que pour une dur?e d'une seule trame cela se limitera ? des marqueurs au
m?me *starttime*).





La commande ***sequence*** demande l'importation d'un fichier avec une
s?quence de images (trames vid?o). Elle a un seul argument obligatoire;
*src*, qui sp?cifie le nom du fichier source. Ce nom peut ?tre donn? en
absolu, avec un chemin d'acc?s complet, ou en relatif par rapport au
r?pertoire dans lequel se trouve le fichier .QI2M. **NB**: sous MS Windows,
un *chemin relatif faisant r?f?rence ? un r?pertoire parent* ("..\") n'est
*pas* support? actuellement du ? une anomalie dans QuickTime.

Si plusieurs commandes ***sequence*** sont donn?es dans un fichier .QI2M, les
diff?rentes sources sp?cifi?es sont import?es, chacune dans sa propre piste
de la vid?o, mais en respectant au mieux l'horodatage des originaux (ou comme
QuickTime le juge bien si *asmovie=True*; cf. ci-dessous).

NB: en cas d'erreur d'importation d'une s?quence, la version pour MS Windows
permettra de continuer l'importation des autres s?quences, ou d'annuler
l'op?ration.





Contr?le de l'importation: r?glages





QuickTime ne fournit pas de m?canisme standard pour contr?ler
(optionellement!) le comportement d'une importation. Il y a deux r?glages qui
s'appliquent au niveau globale de l'importation, les arguments ? la section
***import*** ci-dessus:





**autoSave** (boolean) : si *True*, un fichier .MOV qui fait r?f?rence au
fichier(s) import?(s) ("cache") est enregistr? automatiquement. Ce fichier ,
bien plus petit que l'original, s'ouvrira dans QuickTime en absence du plugin
QTImage2Mov, et surtout quasi instantan?ment. Le nom du fichier est obtenu du
nom du fichier .QI2M en rempla?ant l'extension QI2M par MOV.
Par d?faut QTImage2Mov est compil? de fa?on ? ce que ce fichier est ouvert au
d?but du processus d'importation, au lieu de quand l'importation s'est
termin?e sans fautes. Cela a l'avantage que les pistes TimeCode et texte sont
cr??es d'une fa?on qui permet d'y faire r?f?rence dans d'autres *movies*
(comme le fait QTVODm2), au lieu de les importer en entier.

**autoSaveName** (string) : si pr?sent, la chaine qui sera utilis?e comme le
nom du fichier cr?e si **autoSave=True**.

**askSave** (boolean) : si *True*, une fen?tre dialogue est pr?sent?e apr?s
l'importation qui permet d'enregistrer la vid?o r?sultante avant m?me qu'elle
ne s'affiche. Cette option est surtout int?ressante si on veut transcoder le
fichier .VOD en h.264 par exemple (et qu'on ne dispose pas de la licence
"QuickTime Pro" qui d?bloque la m?me fonctionnalit? dans le *QuickTime
Player*). Elle permet aussi d'enregistrer la vid?o import?e en tant que
fichier .MOV autonome (incluant toutes les donn?es et ressources,
contrairement ? un fichier .MOV de r?f?rence).





Les r?glages qui contr?lent l'importation des fichiers de s?quence
individuels:





**freq** (flottant) : fr?quence d'enregistrement d'un fichier .VOD en Hz; -1
pour une estimation automatique. L'estimation automatique obtient un petit
nombre d'?chantillons du nombre d'images enregistr?es ? la m?me seconde,
pendant la premi?re dizaine de secondes de l'enregistrement, et en calcule
une moyenne en excluant les extr?mes.
NB: la fr?quence utilis?e pour la piste TimeCode est une valeur enti?re qui
n'est pas inf?rieure ? la vraie fr?quence.

**interval** (entier) : l'intervalle d'importation des trames; -1 ou 1 pour
importer toutes les trames. Cette variable modifie la fr?quence de la vid?o
pour pr?server la dur?e.

**maxframes** (entier) : permet de limiter le nombre de trames ? importer.
Omettre ou sp?cifier une valeur n?gative pour importer toutes les trames.

**starttime** (boolean) : si *True*, la 1e trame de la vid?o aura le temps du
d?but de l'enregistrement d'origine. Mieux mettre sur *False* pour un
transcodage (qui ne pr?servera pas la piste TimeCode!), mais ? laisser sur
*True* s'il faut synchroniser avec d'autres donn?es dont on connait le temps
absolut d'?chantillonnage!

**timepad** (boolean) : il y a deux fa?ons de pr?ciser un temps non-z?ro de
la premi?re trame d'une vid?o. ? priori l'ajout d'une piste TimeCode ? la
piste du fichier .VOD suffit (*timepad=False*) mais ces informations ne sont
pas forc?ment disponibles pour un contr?le programmatique de la lecture
vid?o. Pour cette raison une deuxi?me fa?on est propos?e (*timepad=True*),
dans laquelle la piste du fichier .VOD contiendra un "trou" qui commence ?
00:00:00 (minuit) et dure jusqu'au d?but de l'enregistrement. Il s'agit donc
d'une "astuce" qui permet de lever la distinction entre temps relatif et
temps absolut.
NB: Il est fait de sorte que la vid?o s'ouvre ? son vrai d?but, et non pas ?
t=0s!

**transH,transV** (entier) : translation horizontale et/ou verticale, par
rapport ? l'origine de la vid?o ( (0,0); en haut ? gauche). Cette origine
reste toujours ? (0,0); il n'est donc pas possible de faire une translation
n?gative (? gauche ou en haut de l'origine), et une translation n'aura pas
d'effet visible si seulement une seule s?quence est import?e.

**relTransH,relTransV** (flottant) : translation horizontale et/ou verticale
comme d?crit pour *transH,transV*, mais sp?cifi?e en r?f?rant ? la taille de
la s?quence (facteur d'?chelle).

**hidetc** (boolean) : rend la/les pistes TimeCode actuellement pr?sentes
dans le Movie invisible. Mis en oeuvre en mettant la largeur de ces pistes ?
z?ro.

**hidets** (boolean) : d?sactive la piste *timeStamp* qui vient
(potentiellement) d'?tre import?e invisible. Contrairement ? **hidets**, ce
r?glage n'a donc pas d'incidence sur les pistes timeStamp d?j? import?es.

**channel** (entier) : un fichier .VOD peut contenir les flux provenant de
jusqu'? 4 cam?ras; ce r?glage sp?cifie le canal du quad ? s?lectionner : 1 =
haut-gauche, 4 = bas-droite ou -1 pour l'image compl?te. Cette s?lection
s'op?re sur la piste contenant la s?quence actuelle et sur aucune autre
piste. Il est donc pr?f?rable de cacher les pistes TimeCode via la commande
**hidetc=True**. (Une piste TimeCode peut correspondre ? plusieurs pistes
vid?o.)
Le r?glage **channel=5** s?lectionne uniquement la piste TimeCode du Movie.
Le r?glage **channel=6** s?lectionne uniquement la piste texte du Movie
(contient les horodatages individuels et les infos GPS pour les fichiers
VOD).

**hflip** (boolean) : permet de faire une inversion horizontale de l'image
compl?te, pour annuler inversion produite par les cam?ras lat?rales.

**vmgi** (boolean) : pour un fichier VOD, si la structure VMGI doit ?tre
utilis?e. Il n'y a pas de raison de modifier cette param?tre, sauf en cas de
probl?mes d'importation! (d?faut = *True*)

**log** (boolean) : maintient un fichier journal (*toto.VOD.log*) du progr?s
de lecture du fichier *toto.VOD* . Ce fichier contiendra une trace des trames
lues et ignor?es, avec leur horodatage.

**newchapter** (boolean) : si *True*, g?n?re un marqueur de chapitre avec le
nom de fichier source au d?but de la s?quence. En plus, une entr?e est faite
pour la 1e trame avec un horodatage GPS valide.

**description** (string) : une description qui sera associ?e ? la piste dans
laquelle la s?quence actuelle est import?e. Le texte ne peut pas contenir des
caract?res avec accents!

**asmovie** (boolean) : si *True*, le fichier source est import? via
QuickTime. En autres termes, l'importation se passera comme si le fichier
source avait ?t? ouvert dans le lecteur QuickTime ou via l'Exploreur
(Finder).

**fcodec** (string) : permet de sp?cifier le codec utilis? par *ffmpeg* pour
convertir le contenu d'une vid?o VOD de type MPEG4 en une vid?o d'importation
temporaire. Par d?faut, le codec utilis? est *copy*, c-a-d que la s?quence
n'est pas transcod?e mais simplement copi?e. On peut mettre ici tout codec
support? par *ffmpeg *(on obtient la liste en tapant *ffmpeg -encoders* dans
une fen?tre 'invit? de commandes') mais tous les codecs ne sont pas support?s
par QuickTime. On peut obtenir du *Motion JPEG* (le format des VODs anciens)
avec le codec *mjpeg* (pour obtenir une vid?o qui lit de fa?on plus fluide en
QTVODm2).

**fbitrate** (string) : permet de contr?ler le taux (*bit rate*) dans la
vid?o d'importation (uniquement quand un codec est sp?cifi? ?galement). Le
taux est un nombre, mais on peut sp?cifier les milliers par le lettre k, par
exemple 1000k indique un taux de 1000 kilo bits par seconde.

**fsplit** (boolean) : d?termine la fa?on d'importation des fichiers VOD en
format MPEG4. Par d?faut (*fsplit=False*, *fcodec=copy* ou non d?fini), la
vid?o de ces fichiers est import?e sans transcodage, dans une seule piste
dans laquelle les vues des 4 cam?ras sont arrang?es en 'quad' dans l'image.
Si *fsplit=True*, la vid?o est transcod?e et d?coup?e de fa?on ? avoir 4
pistes avec chacune la vue d'une cam?ra, la 1e piste la vue de la cam?ra 1
(quart haut-gauche), la 2e la vue de la cam?ra 2 (quart haut-droit), la 3e le
quart bas-gauche et la 4e piste la vue de la cam?ra 4 (quart bas-droit). Si
*fcodec* n'est pas d?fini, la codec mjpeg sera utilis?, avec un taux
(*fbitrate*) estim? pour pr?serv? le taux d'origine. Dans ce mode
d'importation, les 4 pistes g?n?r?es re?oivent l'identification de la cam?ra
dans les meta-donn?es avec la cl? 'cam#' ("Camera 1", "Camera 2", etc.). La
lecture dans le lecteur QTVODm2 est plus fluide dans ce mode, au d?triment du
temps de la premi?re importation.





NB: dans une vid?o QuickTime, le temps est sp?cifi? dans une unit? qui est,
par d?faut, de 600 par seconde (la *TimeScale*). Pour une meilleure
int?gration/synchronisation avec des applications scientifiques, QTImage2Mov
cr?e des vid?os avec unit? de 1000/seconde par d?faut, 1000**fr?quence* si la
1e piste import?e est un fichier VOD.

NB: faute d'informations, le son des fichiers .VOD n'est pas import?
actuellement. En effet, bien qu'il est trivial d'extraire le son en soi, il
manque ? l'heure actuelle les informations permettant de s?lectionner la
partie pertinente et d'assurer la synchronisation avec la vid?o du fichier.





Fonctionnalit?s et commandes suppl?mentaires





La commande ***sequence*** peut ?tre utilis?e pour importer n'importe quelle
source audio-visuelle que QuickTime sait lire (que ce soit un format
"interne" ou un format disponible via un autre *Importer Component*).
QTImage2Mov fournit un format indig?ne : le format ***JPGS*** (le pluriel de
jpg). Ce format d?crit un flux (s?quence) d'images (trames) tr?s simple. Un
fichier .JPGS contient une s?rie de champs (dans l'ordre):





frame=<codec> # commentaire
rect=<Largeur>x<Hauteur>x<Profondeur>@<dpiX>x<dpiY> size=<taille>
time=<dur?e> <donn?es binaires> frame= ...



Ici, le *<codec>* est le format de l'image (qui doit ?tre le m?me pour toutes
les trames du m?me fichier!); il peut ?tre *JPEG*, *TIFF*, .*PNG*, .*BMP*,
.*SGI*, *.RAW* (pour des images non compress?es) ou .*RGB* (pour les images
"SGI") et .*GIF* (notez le point devant les types ? 3 caract?res!). Il est
permis de mettre un *#* suivi d'un commentaire apr?s la sp?cification du
codec. Les dimensions graphiques de l'image sont sp?cifi?es sur la ligne
*rect=*; largeur et hauteur en pixels, et profondeur en bits, suivi de la
r?solution horizontale et verticale en pixels par pouce (typiquement 72!).
Par exemple :





rect=1280x1024x24@72x72





La taille, *size*, donne le nombre d'octets qu'occupe l'image proprement
dite. La dur?e de pr?sentation de la trame est sp?cifi?e, en secondes, sur la
ligne *time=*.

Apr?s avoir lu cette derni?re ligne, *<taille>* octets sont attendus/suppos?s
constituer l'image. Si la trame n'est pas la seule ou derni?re de la
s?quence, l'image est suivi par un retour ? la ligne, et puis une nouvelle
trame d?finie par la s?rie *frame,rect,size,time*.

Ce format permet de g?n?rer des s?quences vid?o facilement ? partir d'un
logiciel qui g?n?re des images dans un format support?, et peut ?tre lu
extr?mement vite par QTImage2Mov. En interne, le traitement des trames est
identique pour les fichiers .VOD et .JPGS: une routine de haut niveau re?oit
les adresses de d?but des images cons?cutives, leur taille, dimensions et
format, et en construit un tableau ? partir duquel QuickTime construit une
piste vid?o. Cette piste contiendra la liste des adresses dans le fichier
source et toute autre information permettant de visualiser la vid?o.





QTImage2Mov a ?t? d?velopp? ? partir d'un exemple nomm?
*QTSlideShowImporter*. Cet *Importer Component* permet de cr?er, via un
fichier XML, un diaporama ? partir d'une ou plusieurs images et d'y ajouter
une piste son. Cette fonctionnalit? a ?t? pr?serv?e et est disponible via les
commands ***image*** et ***audio***.





<image src="fichier" dur=<sec> mdur=<millisec> ismovie=False >

**dur** (entier) : dur?e de pr?sentation en secondes

**mdur** (entier) : dur?e de pr?sentation en millisecondes

**ismovie** (entier) : si *non-z?ro*, le fichier est traite comme une vid?o.





<audio src="fichier" >

L'audio dans le fichier indiqu? est ajout? dans *la* piste son. Cette
commande n'est prise en compte qu'une seule fois (la premi?re).





Fichiers VOD MPEG4





Les lecteurs Brigade de plus r?cente g?n?ration compriment la vid?o
enregistr?e en MPEG4. Contrairement aux fichiers de l'ancienne g?n?ration,
les nouveaux fichiers ne contiennent donc plus des trames qui sont en fait
des images JPEG compl?tes. ? la place, ils contiennent des fragments MPEG4
d'une dur?e de 1 seconde et qui contiennent ? leur tour *F* images (trames),
o? *F* est la fr?quence d'enregistrement. Ce changement a provoqu? plusieurs
adaptations largement transparentes dans QTImage2Mov. L? o? QuickTime est
capable d'afficher sans aide les trames JPEG, obtenues directement dans le
fichier VOD d'origine, il n'en est pas pareil pour le MPEG4. D'une part, il
s'agit d'MPEG4 brut (*m4v part 2* pour ?tre pr?cis) et non pas de petites
*movies* compl?tes. D'autre part, m?me si on renseigne QuickTime directement
o? trouver les trames individuelles (ce qui requiert une librairie comme
fournie par le projet *FFmpeg*), rien n'est pr?vu par d?faut pour les
d?coder. Au moment du d?veloppement de QTImage2Move, un d?codeur existait
uniquement pour Mac OS X (*Perian*) et aurait du ?tre d?velopp? pour MS
Windows, toujours ? l'aide des librairies du projet *FFmpeg*.

Il a donc ?t? d?cid? d'utiliser une solution moins ?l?gante, qui d?pend
?galement, mais diff?remment, du projet *FFmpeg*. Il est en effet possible
d'extraire les fragments MPEG4 du fichier VOD, et les "re-encapsuler" (sans
transcodage) pour en faire un fichier .mp4 ou .mov qui ne contient **que**
les donn?es vid?o. Cette op?ration se fait avec l'utilitaire *ffmpeg* et
donne lieu ? une vid?o temporaire d'importation dans le m?me r?pertoire*. *La
vid?o r?sultante peut ensuite ?tre import?e pour l'int?grer avec les donn?es
temporelles et de g?olocalisation. C'est cette solution qui a ?t? retenue.

? l'ouverture d'un fichier .VOD qui contient du MPEG4, un scan est fait pour
estimer la fr?quence d'enregistrement, en occurrence en utilisant
l'utilitaire *ffprobe*. Ensuite, les fragments MPEG4 sont extraites et pass?s
? *ffmpeg* (ex?cution en parall?le) tandis que les donn?es d'horodatage et du
GPS sont stock?es dans le *Movie* en construction. La vid?o temporaire
d'importation est ensuite import?e dans le *Movie*, puis supprim?e du disque.

Le *Movie* g?n?r? contiendra la m?thode d'importation sous la cl? 'quad' dans
ses m?ta-donn?es : *"MPG4 VOD imported as 4 tracks"* ou *"MPG4 VOD imported
as a single track"* (cf. *fsplit*).





Les fichiers VOD MPEG4 donnent donc lieu ? des *Movies* qui contiennent tout
le contenu d'origine, vid?o comme m?ta-donn?es ? et les fichiers VOD peuvent
donc ?tre archiv?s. En contraste, les anciens fichiers VOD s'importent *par
r?f?rence*, c-a-d qu'ils doivent ?tre pr?sent pour pouvoir visionner les
*Movies* import?s (qui sont donc bien plus petits que les fichiers
d'origine).





? noter:

les utilitaires *ffmpeg* et *ffprobe* doivent ?tre disponibles sur le *path*
de MS Windows.

les fragments MPEG4 sont en quelque sorte les trames du fichier VOD et ont
une dur?e de 1 seconde. Puisqu'ils sont import?s tel quel, la piste avec
l'horodatage a donc une r?solution de 1 seconde.

les enregistreurs VOD MPEG4 ne fonctionnent pas ? 12,5Hz mais ? 12Hz.

l'op?ration d'importation de la vid?o temporaire d'importation peut prendre
plusieurs minutes sur un ordinateur lent (ou ne disposant que de peu de
m?moire vive). C'est une fonction QuickTime qui prend son temps; le progr?s
de l'op?ration est affich? a priori. Toutefois, cette op?ration se passe
beaucoup plus vite si on importe via un fichier .qi2m avec **autoSave=True**
(comme est le cas avec QTVODm2).

La composante d?codeur *FFusion* am?liore la lecture des vid?os MPEG4 de
fa?on significative.

Si on importe le VOD (MPEG4) directement avec *Quicktime Player*, et on
enregistre le r?sultat ensuite, le lecteur est susceptible de terminer avec
une erreur **apr?s la fin** de la sauvegarde. Les fichiers g?n?r?s ne
semblent pas provoquer ce genre de comportement non d?sirable, qui ne se
produit pas non plus quand on utilise les fonctions d'enregistrement
propos?es via les fichiers qi2m.





Installation





QTImage2Mov est une extension pour Apple QuickTime, et QuickTime doit donc
?tre install? sur l'ordinateur h?te.

***MS Windows:***

QuickTime se t?l?charge ? partir de la page

`*http://www.apple.com/quicktime/download/*`_

***NB***: QuickTime est actuellement en train d'?voluer sur sa plate-forme
d'origine, le Macintosh. Pour l'instant ces changements sont sans incidence
sur QTImage2Mov, surtout pas sous MS Windows. Il semble cependant fort
judicieux de pr?server toujours l'installeur (.exe) de la version test?e (et
donc d'utiliser l'outil de MAJ Apple uniquement pour la notification et de
passer par la page web cit?e ci-dessus pour le t?l?chargement). La version
actuelle est v7.7.3 (fonctionne, le 16/11/2012).





***Mac OS X:***

QuickTime X est install? par d?faut; il se peut qu'il soit n?cessaire
d'installer la version pr?c?dente (7) en parall?le:

`*http://support.apple.com/kb/DL923*`_





Pour installer QTImage2Mov, il suffit de copier un ?l?ment dans le r?pertoire
d'installation de QuickTime (manuellement; instructions ci-dessous).
?videmment il faut quitter toute application utilisant QuickTime au
pr?alable. Apr?s l'installation, la fonctionnalit? d'importation est
imm?diatement disponible en ouvrant un fichier .VOD ou .QI2M ? partir d'une
application QuickTime. L'association entre les extensions (.VOD, .QI2M,
.JPGS) et une application par d?faut (par exemple *QuickTime Player*) doit se
faire manuellement, avec les m?thodes mises ? disposition par le syst?me
d'exploitation.





***MS Windows*****:**

QTImage2Mov prend la forme d'un fichier unique, qui est ? installer dans le
sous-r?pertoire ***QTComponents*** du r?pertoire d'installation de QuickTime.
Deux versions sont fournies, aux fonctionnalit?s l?g?rement diff?rentes (?
choisir):





**QTImage2Mov-dev.qtx** : version "de d?veloppement". Cette version exploite
la librairie SS_Log `*(http://www.codeproject.com/KB/macros/ss_log.aspx*`_)
pour donner un aper?u de l'avancement d'une importation et/ou afficher
certains messages d'erreur. Elle n?cessite la pr?sence d'une dll et une
application sur le *path* (*SS_Log_AddIn.dll* et *SS_Log_Window.exe* par
exemple dans le r?pertoire Windows). Une fen?tre avec barre d'avancement est
?galement pr?sent?e lors d'une importation qui prend plus de 5 secondes;
cette fen?tre permet d'interrompre le processus.

**QTImage2Mov.qtx** : version "de production". Cette version pr?sente
uniquement la fen?tre avec barre d'avancement lors d'une importation longue.

Pour l'importation de fichiers VOD MPEG4, les utilitaires *ffmpeg* et
*ffprobe* du projet *FFmpeg* doivent ?tre install?s:

`*http://ffmpeg.zeranoe.com/builds/*`_ (prendre de pr?f?rence le *build*
statique, si possible en 64 bits).

Le code source se trouve aux chemins suivants :

\\pandore\USBShare\msis\rjvb\QuickTimeStuff\brigade
\\pandore\USBShare\msis\rjvb\QuickTimeStuff\QTImage2Mov ftp://`*anonymous@ftp
.inrets.fr*`_/incoming/FtpSimU/logiciels/RJVB/brigade.tar.bz2 ftp://`*anonymo
us@ftp.inrets.fr*`_/incoming/FtpSimU/logiciels/RJVB/QTImage2Mov.tar.bz2
ftp://`*anonymous@ftp.inrets.fr*`_/incoming/FtpSimU/logiciels/RJVB/QuickTime-
Installers/QuickTimeSDK-73.zip \\pandore\USBShare\msis\rjvb\Libs

Pour la compilation, il faut installer le SDK de QuickTime ? l'endroit
propos? par d?faut (C:\Program Files) puis utiliser la solution Microsoft
Visual Studio 2010 (Express), QTImage2MovVS2010.sln . La version
'd?veloppeur' utilise SS_Log pour afficher des messages de suivi; le code
n?cessaire est attendu dans C:\Libs .





***Mac OS X*****:**

QTImage2Mov prend la forme d'un *bundle*: **QTImage2Mov.component**. Cet
?l?ment est ? placer dans **/Library/QuickTime** ou dans le dossier
**QuickTime** dans la **Biblioth?que** dans votre dossier de d?part
(**~/Library/QuickTime**).

*FFMpeg* peut ?tre install? via MacPorts ou Fink.

D'autres notes (plut?t concernant le d?veloppement) sont disponibles dans le
fichier *TODOHIST* dans le r?pertoire du code source.

1 cf. `*http://developer.apple.com/library/mac/#documentation/QuickTime/R
    M/MovieBasics/MTEditing/I-Chapter/9TimecodeMediaHandle.html*`_1
    Actuellement ignor? et brid? ? l'?quivalent de 1 trame de la vid?o.1 ?
    partir de la version 12, la piste TimeCode est cach?e par d?faut (et la
    piste timeStamp visible) en mettant *sa hauteur ? 1 pixel*. Ceci permet
    de la rendre visible ? nouveau, contrairement ? un redimensionnement ? 0
    pixels.2 Les pistes *timeStamp* sont sp?cifiques ? la s?quence import?e
    et il peut donc y avoir plusieurs de ces pistes. Par contre, les donn?es
    TimeCode de toutes les s?quences import?es sont enregistr?es dans une
    seule piste TimeCode, et il ne peut donc y avoir plusieurs de ces pistes
    *que* quand on importe des s?quences (.mov) enti?res qui en contiennent
    d?j?.

.. _http://www.apple.com/quicktime/download/:
    http://www.apple.com/quicktime/download/
.. _http://support.apple.com/kb/DL923: http://support.apple.com/kb/DL923
.. _(http://www.codeproject.com/KB/macros/ss_log.aspx:
    http://www.codeproject.com/KB/macros/ss_log.aspx
.. _http://ffmpeg.zeranoe.com/builds/: http://ffmpeg.zeranoe.com/builds/
.. _anonymous@ftp.inrets.fr: mailto:anonymous@ftp.inrets.fr
.. _http://developer.apple.com/library/mac/#documentation/QuickTime/RM/Mo
    vieBasics/MTEditing/I-Chapter/9TimecodeMediaHandle.html: http://developer
    .apple.com/library/mac/#documentation/QuickTime/RM/MovieBasics/MTEditing/
    I-Chapter/9TimecodeMediaHandle.html
