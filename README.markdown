**QTImage2Mov ([****QT** **I****mage****To** **M****ovie])
-
version 1.20**

    QTImage2Mov is an importer component for XML containers listing import commands, as well as
    for a very simple image stream container format (JPGS) and a proprietary video format (VOD).
    The XML container format allows to specify one or multiple import files as well as to configure the import procedure (e.g. mask quadrants or flip) and add chapters or other metadata.
    Note that this repository does NOT contain the VOD import code which cannot at this time be made public.

QTImage2Mov est un plug-in pour le "noyau audio-visuel" QuickTime® ("Importer Component" dans le jargon d'Apple) qui permet d'importer certains types de fichiers — notamment des fichiers .VOD enregistrés sur les enregistreurs Brigade — et d'en faire des vidéos QuickTime. Cette approche a l'intérêt que, sans effort supplémentaire, les fichiers .VOD ainsi que les vidéos résultantes s'ouvrent avec le lecteur QuickTime ou tout autre application faisant appel à QuickTime, mise à part du fait que QuickTime fournit gratuitement une multitude de fonctionnalités multimédia.

Nouveauté de la version 1.20 : l'importation des fichiers VOD contenant du MPEG4.

L'importation donne lieu à un objet en mémoire vive (un *Movie*) qui représente et prend la forme d'une vidéo sur l'écran, dans laquelle sont affichées les trames du fichier importé (un fichier .VOD par exemple). On peut ouvrir directement un fichier .VOD. Dans ce cas, le fichier s'ouvre avec un jeu de réglages par défaut (décrits ci-dessous). Si ces réglages ne conviennent pas, par exemple s'il faut ouvrir seulement le flux d'une des 4 caméras enregistrées dans un fichier .VOD, il faut passer par un fichier .QI2M (pour QtImage2Mov). Ce fichier est un document XML qui précise quel fichier(s) importer, et comment. Cette "interface de réglage" a été choisi pour sa facilité (pas besoin de concevoir une interface graphique; création de façon programmatique, ...) et son "universalité". Sa forme basique peut ressembler à ceci:

\<?xml version="1.0"?\>

\<?quicktime type="video/x-qt-img2mov"?\>

\<!-- commentaire --\>

\<import askSave=False autoSave=False \>

\<description txt="ligne 1 (sans accents!)" lang="fr\_FR" /\>

\<chapter src="titre du chapitre" /\>

\<sequence src="tstVOD.VOD" freq=-1 interval=-1 maxframes=-1

channel=-1 starttime=True  hidetc=False hidets=False timepad=False

transH=0 transV=0 relTransH=0 relTransV=0 hflip=False  vmgi=True log=False description="une description"  asmovie=False newchapter=True /\>

\</import\>

Ici, on importe un seul fichier tstVOD.VOD avec les réglages par défaut (voir ci-dessous) dans une piste dédiée d'une nouvelle vidéo QuickTime. La piste et la vidéo entière reçoivent chacune une description individuelle. Le 'movie' contiendra une piste *TimeCode*<sup></sup> qui sera visible et affichera une indication du temps d'enregistrement de chaque trame dans le format heure:min:sec;trames. En plus, les horodatages de chaque trame (image) ainsi que des données GPS sont stockés dans une piste texte (*timeStamp*) quand la source contient ces informations (ce qui est le cas pour les fichiers VOD). Si les informations GPS existent, le texte est en gras si les données sont valides (bonne réception GPS), sinon en 'normal' (dans ce cas les valeurs ne sont pas forcément mises à jour jusqu'à ce le GPS captait à nouveau correctement). Cette piste est invisible par défaut (cf. le réglage d'importation **channel**, ci-dessous).

Les descriptions ainsi que certaines informations pertinentes concernant le fichier source (tstVOD.VOD) sont stockées dans les meta-données de la vidéo et consultables via l'API QuickTime.

Il est important de noter que le comportement par défaut est celui d'un lecteur, sans effets secondaires: la vidéo est ouverte (importée), mais rien n'est modifié ou sauvegardé sur disque.

Structure du fichier xml .QI2M

Le fichier .QI2M comporte une section principale, ***import***, qui regroupe la ou les commandes d'importation qui donneront lieu aux données audio-visuelles de la vidéo. Les commandes principales sont décrites ci-dessous.

Commandes principales dans la section ***import***

La commande ***description*** ajoute des informations textuelles à la vidéo; le texte spécifié par chaque commande est ajouté à la description déjà existante. On peut éventuellement préciser le langage utilisé, mais on ne peut pas utiliser des caractères avec accents.

La commande ***chapter*** demande la création d'un marqueur de chapitre au temps actuel dans la vidéo. Le contrôleur de QuickTime affiche ces marqueurs dans un menu 'popup', permettant une navigation rapide dans une vidéo à plusieurs pistes consécutives. Cette commande accepte deux paramètres optionnels: *starttime=t* permet de spécifier le temps relatif au début de la vidéo (en secondes) où le marqueur doit être inséré et *duration=d* permet de préciser la durée<sup></sup>. Attention si un titre est inséré à un temps déjà existant dans la piste (au lieu d'être ajouté à la fin de la piste). Dans ce cas, le segment correspondant à *starttime* et *duration* est supprimé de la piste qui contient les marqueurs de chapitre pour éviter un décalage d'éventuels marqueurs suivants (et il est donc possible de supprimer un marqueur existant bien que pour une durée d'une seule trame cela se limitera à des marqueurs au même *starttime*).

La commande ***sequence*** demande l'importation d'un fichier avec une séquence de images (trames vidéo). Elle a un seul argument obligatoire; *src*, qui spécifie le nom du fichier source. Ce nom peut être donné en absolu, avec un chemin d'accès complet, ou en relatif par rapport au répertoire dans lequel se trouve le fichier .QI2M. **NB**: sous MS Windows, un *chemin relatif faisant référence à un répertoire parent* ("..\\") n'est pas supporté actuellement du à une anomalie dans QuickTime.

Si plusieurs commandes ***sequence*** sont données dans un fichier .QI2M, les différentes sources spécifiées sont importées, chacune dans sa propre piste de la vidéo, mais en respectant au mieux l'horodatage des originaux (ou comme QuickTime le juge bien si *asmovie=True*; cf. ci-dessous).

NB: en cas d'erreur d'importation d'une séquence, la version pour MS Windows permettra de continuer l'importation des autres séquences, ou d'annuler l'opération.

Contrôle de l'importation: réglages

QuickTime ne fournit pas de mécanisme standard pour contrôler (optionellement!) le comportement d'une importation. Il y a deux réglages qui s'appliquent au niveau globale de l'importation, les arguments à la section ***import*** ci-dessus:

**autoSave** (boolean) : si *True*, un fichier .MOV qui fait référence au fichier(s) importé(s) ("cache") est enregistré automatiquement. Ce fichier , bien plus petit que l'original, s'ouvrira dans QuickTime en absence du plugin QTImage2Mov, et surtout quasi instantanément. Le nom du fichier est obtenu du nom du fichier .QI2M en remplaçant l'extension QI2M par MOV. Par défaut QTImage2Mov est compilé de façon à ce que ce fichier est ouvert au début du processus d'importation, au lieu de quand l'importation s'est terminée sans fautes. Cela a l'avantage que les pistes TimeCode et texte sont créées d'une façon qui permet d'y faire référence dans d'autres *movies* (comme le fait QTVODm2), au lieu de les importer en entier.

**autoSaveName** (string) : si présent, la chaine qui sera utilisée comme le nom du fichier crée si **autoSave=True**.

**askSave** (boolean) : si *True*, une fenêtre dialogue est présentée après l'importation qui permet d'enregistrer la vidéo résultante avant même qu'elle ne s'affiche. Cette option est surtout intéressante si on veut transcoder le fichier .VOD en h.264 par exemple (et qu'on ne dispose pas de la licence "QuickTime Pro" qui débloque la même fonctionnalité dans le *QuickTime Player*). Elle permet aussi d'enregistrer la vidéo importée en tant que fichier .MOV autonome (incluant toutes les données et ressources, contrairement à un fichier .MOV de référence).

Les réglages qui contrôlent l'importation des fichiers de séquence individuels:

**freq** (flottant) : fréquence d'enregistrement d'un fichier .VOD en Hz; -1 pour une estimation automatique. L'estimation automatique obtient un petit nombre d'échantillons du nombre d'images enregistrées à la même seconde, pendant la première dizaine de secondes de l'enregistrement, et en calcule une moyenne en excluant les extrêmes. NB: la fréquence utilisée pour la piste TimeCode est une valeur entière qui n'est pas inférieure à la vraie fréquence.

**interval** (entier) : l'intervalle d'importation des trames; -1 ou 1 pour importer toutes les trames. Cette variable modifie la fréquence de la vidéo pour préserver la durée.

**maxframes** (entier) : permet de limiter le nombre de trames à importer. Omettre ou spécifier une valeur négative pour importer toutes les trames.

**starttime** (boolean) : si *True*, la 1e trame de la vidéo aura le temps du début de l'enregistrement d'origine. Mieux mettre sur *False* pour un transcodage (qui ne préservera pas la piste TimeCode!), mais à laisser sur *True* s'il faut synchroniser avec d'autres données dont on connait le temps absolut d'échantillonnage!

**timepad** (boolean) : il y a deux façons de préciser un temps non-zéro de la première trame d'une vidéo. À priori l'ajout d'une piste TimeCode à la piste du fichier .VOD suffit (*timepad=False*) mais ces informations ne sont pas forcément disponibles pour un contrôle programmatique de la lecture vidéo. Pour cette raison une deuxième façon est proposée (*timepad=True*), dans laquelle la piste du fichier .VOD contiendra un "trou" qui commence à 00:00:00 (minuit) et dure jusqu'au début de l'enregistrement. Il s'agit donc d'une "astuce" qui permet de lever la distinction entre temps relatif et temps absolut. NB: Il est fait de sorte que la vidéo s'ouvre à son vrai début, et non pas à t=0s!

**transH,transV** (entier) : translation horizontale et/ou verticale, par rapport à l'origine de la vidéo ( (0,0); en haut à gauche). Cette origine reste toujours à (0,0); il n'est donc pas possible de faire une translation négative (à gauche ou en haut de l'origine), et une translation n'aura pas d'effet visible si seulement une seule séquence est importée.

**relTransH,relTransV** (flottant) : translation horizontale et/ou verticale comme décrit pour *transH,transV*, mais spécifiée en référant à la taille de la séquence (facteur d'échelle).

**hidetc** (boolean) : rend la/les pistes TimeCode actuellement présentes dans le Movie invisible. Mis en oeuvre en mettant la largeur de ces pistes à zéro<sup></sup>.

**hidets** (boolean) : désactive la piste *timeStamp* qui vient (potentiellement) d'être importée invisible. Contrairement à **hidets**, ce réglage n'a donc pas d'incidence sur les pistes timeStamp déjà importées<sup></sup>.

**channel** (entier) : un fichier .VOD peut contenir les flux provenant de jusqu'à 4 caméras; ce réglage spécifie le canal du quad à sélectionner : 1 = haut-gauche, 4 = bas-droite ou -1 pour l'image complète. Cette sélection s'opère sur la piste contenant la séquence actuelle et sur aucune autre piste. Il est donc préférable de cacher les pistes TimeCode via la commande **hidetc=True**. (Une piste TimeCode peut correspondre à plusieurs pistes vidéo.) Le réglage **channel=5** sélectionne uniquement la piste TimeCode du Movie. Le réglage **channel=6** sélectionne uniquement la piste texte du Movie (contient les horodatages individuels et les infos GPS pour les fichiers VOD).

**hflip** (boolean) : permet de faire une inversion horizontale de l'image complète, pour annuler inversion produite par les caméras latérales.

**vmgi** (boolean) : pour un fichier VOD, si la structure VMGI doit être utilisée. Il n'y a pas de raison de modifier cette paramètre, sauf en cas de problèmes d'importation! (défaut = *True*)

**log** (boolean) : maintient un fichier journal (*toto.VOD.log*) du progrès de lecture du fichier *toto.VOD* . Ce fichier contiendra une trace des trames lues et ignorées, avec leur horodatage.

**newchapter** (boolean) : si *True*, génère un marqueur de chapitre avec le nom de fichier source au début de la séquence. En plus, une entrée est faite pour la 1e trame avec un horodatage GPS valide.

**description** (string) : une description qui sera associée à la piste dans laquelle la séquence actuelle est importée. Le texte ne peut pas contenir des caractères avec accents!

**asmovie** (boolean) : si *True*, le fichier source est importé via QuickTime. En autres termes, l'importation se passera comme si le fichier source avait été ouvert dans le lecteur QuickTime ou via l'Exploreur (Finder).

**fcodec** (string) : permet de spécifier le codec utilisé par *ffmpeg* pour convertir le contenu d'une vidéo VOD de type MPEG4 en une vidéo d'importation temporaire. Par défaut, le codec utilisé est *copy*, c-a-d que la séquence n'est pas transcodée mais simplement copiée. On peut mettre ici tout codec supporté par *ffmpeg*(on obtient la liste en tapant *ffmpeg -encoders* dans une fenêtre 'invité de commandes') mais tous les codecs ne sont pas supportés par QuickTime. On peut obtenir du *Motion JPEG* (le format des VODs anciens) avec le codec *mjpeg* (pour obtenir une vidéo qui lit de façon plus fluide en QTVODm2).

**fbitrate** (string) : permet de contrôler le taux (*bit rate*) dans la vidéo d'importation (uniquement quand un codec est spécifié également). Le taux est un nombre, mais on peut spécifier les milliers par le lettre k, par exemple 1000k indique un taux de 1000 kilo bits par seconde.

**fsplit** (boolean) : détermine la façon d'importation des fichiers VOD en format MPEG4. Par défaut (*fsplit=False*, *fcodec=copy* ou non défini), la vidéo de ces fichiers est importée sans transcodage, dans une seule piste dans laquelle les vues des 4 caméras sont arrangées en 'quad' dans l'image. Si *fsplit=True*, la vidéo est transcodée et découpée de façon à avoir 4 pistes avec chacune la vue d'une caméra, la 1e piste la vue de la caméra 1 (quart haut-gauche), la 2e la vue de la caméra 2 (quart haut-droit), la 3e le quart bas-gauche et la 4e piste la vue de la caméra 4 (quart bas-droit). Si *fcodec* n'est pas défini, la codec mjpeg sera utilisé, avec un taux (*fbitrate*) estimé pour préservé le taux d'origine. Dans ce mode d'importation, les 4 pistes générées reçoivent l'identification de la caméra dans les meta-données avec la clé 'cam\#' ("Camera 1", "Camera 2", etc.). La lecture dans le lecteur QTVODm2 est plus fluide dans ce mode, au détriment du temps de la première importation.

NB: dans une vidéo QuickTime, le temps est spécifié dans une unité qui est, par défaut, de 600 par seconde (la *TimeScale*). Pour une meilleure intégration/synchronisation avec des applications scientifiques, QTImage2Mov crée des vidéos avec unité de 1000/seconde par défaut, 1000\**fréquence* si la 1e piste importée est un fichier VOD.

NB: faute d'informations, le son des fichiers .VOD n'est pas importé actuellement. En effet, bien qu'il est trivial d'extraire le son en soi, il manque à l'heure actuelle les informations permettant de sélectionner la partie pertinente et d'assurer la synchronisation avec la vidéo du fichier.

Fonctionnalités et commandes supplémentaires

La commande ***sequence*** peut être utilisée pour importer n'importe quelle source audio-visuelle que QuickTime sait lire (que ce soit un format "interne" ou un format disponible via un autre *Importer Component*). QTImage2Mov fournit un format indigène : le format ***JPGS*** (le pluriel de jpg). Ce format décrit un flux (séquence) d'images (trames) très simple. Un fichier .JPGS contient une série de champs (dans l'ordre):

frame=\<codec\> \# commentaire

rect=\<Largeur\>x\<Hauteur\>x\<Profondeur\>@\<dpiX\>x\<dpiY\>

size=\<taille\>

time=\<durée\>

\<données binaires\>

frame= ...

Ici, le *\<codec\>* est le format de l'image (qui doit être le même pour toutes les trames du même fichier!); il peut être *JPEG*, *TIFF*, .*PNG*, .*BMP*, .*SGI*, *.RAW* (pour des images non compressées) ou .*RGB* (pour les images "SGI") et .*GIF* (notez le point devant les types à 3 caractères!). Il est permis de mettre un *\#* suivi d'un commentaire après la spécification du codec. Les dimensions graphiques de l'image sont spécifiées sur la ligne *rect=*; largeur et hauteur en pixels, et profondeur en bits, suivi de la résolution horizontale et verticale en pixels par pouce (typiquement 72!). Par exemple :

rect=1280x1024x24@72x72

La taille, *size*, donne le nombre d'octets qu'occupe l'image proprement dite. La durée de présentation de la trame est spécifiée, en secondes, sur la ligne *time=*.

Après avoir lu cette dernière ligne, *\<taille\>* octets sont attendus/supposés constituer l'image. Si la trame n'est pas la seule ou dernière de la séquence, l'image est suivi par un retour à la ligne, et puis une nouvelle trame définie par la série *frame,rect,size,time*.

Ce format permet de générer des séquences vidéo facilement à partir d'un logiciel qui génère des images dans un format supporté, et peut être lu extrêmement vite par QTImage2Mov. En interne, le traitement des trames est identique pour les fichiers .VOD et .JPGS: une routine de haut niveau reçoit les adresses de début des images consécutives, leur taille, dimensions et format, et en construit un tableau à partir duquel QuickTime construit une piste vidéo. Cette piste contiendra la liste des adresses dans le fichier source et toute autre information permettant de visualiser la vidéo.

QTImage2Mov a été développé à partir d'un exemple nommé *QTSlideShowImporter*. Cet *Importer Component* permet de créer, via un fichier XML, un diaporama à partir d'une ou plusieurs images et d'y ajouter une piste son. Cette fonctionnalité a été préservée et est disponible via les commands ***image*** et ***audio***.

\<image src="fichier" dur=\<sec\> mdur=\<millisec\> ismovie=False \>  
**dur** (entier) : durée de présentation en secondes

**mdur** (entier) : durée de présentation en millisecondes

**ismovie** (entier) : si *non-zéro*, le fichier est traite comme une vidéo.

\<audio src="fichier" \>

L'audio dans le fichier indiqué est ajouté dans *la* piste son. Cette commande n'est prise en compte qu'une seule fois (la première).

Fichiers VOD MPEG4

Les lecteurs Brigade de plus récente génération compriment la vidéo enregistrée en MPEG4. Contrairement aux fichiers de l'ancienne génération, les nouveaux fichiers ne contiennent donc plus des trames qui sont en fait des images JPEG complètes. À la place, ils contiennent des fragments MPEG4 d'une durée de 1 seconde et qui contiennent à leur tour *F* images (trames), où *F* est la fréquence d'enregistrement. Ce changement a provoqué plusieurs adaptations largement transparentes dans QTImage2Mov. Là où QuickTime est capable d'afficher sans aide les trames JPEG, obtenues directement dans le fichier VOD d'origine, il n'en est pas pareil pour le MPEG4. D'une part, il s'agit d'MPEG4 brut (*m4v part 2* pour être précis) et non pas de petites *movies* complètes. D'autre part, même si on renseigne QuickTime directement où trouver les trames individuelles (ce qui requiert une librairie comme fournie par le projet *FFmpeg*), rien n'est prévu par défaut pour les décoder. Au moment du développement de QTImage2Move, un décodeur existait uniquement pour Mac OS X (*Perian*) et aurait du être développé pour MS Windows, toujours à l'aide des librairies du projet *FFmpeg*.

Il a donc été décidé d'utiliser une solution moins élégante, qui dépend également, mais différemment, du projet *FFmpeg*. Il est en effet possible d'extraire les fragments MPEG4 du fichier VOD, et les "re-encapsuler" (sans transcodage) pour en faire un fichier .mp4 ou .mov qui ne contient **que** les données vidéo. Cette opération se fait avec l'utilitaire *ffmpeg* et donne lieu à une vidéo temporaire d'importation dans le même répertoire*.*La vidéo résultante peut ensuite être importée pour l'intégrer avec les données temporelles et de géolocalisation. C'est cette solution qui a été retenue.

À l'ouverture d'un fichier .VOD qui contient du MPEG4, un scan est fait pour estimer la fréquence d'enregistrement, en occurrence en utilisant l'utilitaire *ffprobe*. Ensuite, les fragments MPEG4 sont extraites et passés à *ffmpeg* (exécution en parallèle) tandis que les données d'horodatage et du GPS sont stockées dans le *Movie* en construction. La vidéo temporaire d'importation est ensuite importée dans le *Movie*, puis supprimée du disque.

Le *Movie* généré contiendra la méthode d'importation sous la clé 'quad' dans ses méta-données : *"MPG4 VOD imported as 4 tracks"* ou *"MPG4 VOD imported as a single track"* (cf. *fsplit*).

Les fichiers VOD MPEG4 donnent donc lieu à des *Movies* qui contiennent tout le contenu d'origine, vidéo comme méta-données — et les fichiers VOD peuvent donc être archivés. En contraste, les anciens fichiers VOD s'importent *par référence*, c-a-d qu'ils doivent être présent pour pouvoir visionner les *Movies* importés (qui sont donc bien plus petits que les fichiers d'origine).

À noter:

les utilitaires *ffmpeg* et *ffprobe* doivent être disponibles sur le *path* de MS Windows.

les fragments MPEG4 sont en quelque sorte les trames du fichier VOD et ont une durée de 1 seconde. Puisqu'ils sont importés tel quel, la piste avec l'horodatage a donc une résolution de 1 seconde.

les enregistreurs VOD MPEG4 ne fonctionnent pas à 12,5Hz mais à 12Hz.

l'opération d'importation de la vidéo temporaire d'importation peut prendre plusieurs minutes sur un ordinateur lent (ou ne disposant que de peu de mémoire vive). C'est une fonction QuickTime qui prend son temps; le progrès de l'opération est affiché a priori. Toutefois, cette opération se passe beaucoup plus vite si on importe via un fichier .qi2m avec **autoSave=True** (comme est le cas avec QTVODm2).

La composante décodeur *FFusion* améliore la lecture des vidéos MPEG4 de façon significative.

Si on importe le VOD (MPEG4) directement avec *Quicktime Player*, et on enregistre le résultat ensuite, le lecteur est susceptible de terminer avec une erreur **après la fin** de la sauvegarde. Les fichiers générés ne semblent pas provoquer ce genre de comportement non désirable, qui ne se produit pas non plus quand on utilise les fonctions d'enregistrement proposées via les fichiers qi2m.

Installation

QTImage2Mov est une extension pour Apple QuickTime, et QuickTime doit donc être installé sur l'ordinateur hôte.

**MS Windows:**

QuickTime se télécharge à partir de la page

[http://www.apple.com/quicktime/download/](http://www.apple.com/quicktime/download/)

***NB***: QuickTime est actuellement en train d'évoluer sur sa plate-forme d'origine, le Macintosh. Pour l'instant ces changements sont sans incidence sur QTImage2Mov, surtout pas sous MS Windows. Il semble cependant fort judicieux de préserver toujours l'installeur (.exe) de la version testée (et donc d'utiliser l'outil de MAJ Apple uniquement pour la notification et de passer par la page web citée ci-dessus pour le téléchargement). La version actuelle est v7.7.3 (fonctionne, le 16/11/2012).

**Mac OS X:**

QuickTime X est installé par défaut; il se peut qu'il soit nécessaire d'installer la version précédente (7) en parallèle:

[http://support.apple.com/kb/DL923](http://support.apple.com/kb/DL923)

Pour installer QTImage2Mov, il suffit de copier un élément dans le répertoire d'installation de QuickTime (manuellement; instructions ci-dessous). Évidemment il faut quitter toute application utilisant QuickTime au préalable. Après l'installation, la fonctionnalité d'importation est immédiatement disponible en ouvrant un fichier .VOD ou .QI2M à partir d'une application QuickTime. L'association entre les extensions (.VOD, .QI2M, .JPGS) et une application par défaut (par exemple *QuickTime Player*) doit se faire manuellement, avec les méthodes mises à disposition par le système d'exploitation.

**MS Windows****:**

QTImage2Mov prend la forme d'un fichier unique, qui est à installer dans le sous-répertoire ***QTComponents*** du répertoire d'installation de QuickTime. Deux versions sont fournies, aux fonctionnalités légèrement différentes (à choisir):

**QTImage2Mov-dev.qtx** : version "de développement". Cette version exploite la librairie SS\_Log [(http://www.codeproject.com/KB/macros/ss\_log.aspx](http://www.codeproject.com/KB/macros/ss_log.aspx)) pour donner un aperçu de l'avancement d'une importation et/ou afficher certains messages d'erreur. Elle nécessite la présence d'une dll et une application sur le *path* (*SS\_Log\_AddIn.dll* et *SS\_Log\_Window.exe* par exemple dans le répertoire Windows). Une fenêtre avec barre d'avancement est également présentée lors d'une importation qui prend plus de 5 secondes; cette fenêtre permet d'interrompre le processus.

**QTImage2Mov.qtx** : version "de production". Cette version présente uniquement la fenêtre avec barre d'avancement lors d'une importation longue.

Pour l'importation de fichiers VOD MPEG4, les utilitaires *ffmpeg* et *ffprobe* du projet *FFmpeg* doivent être installés:

[http://ffmpeg.zeranoe.com/builds/](http://ffmpeg.zeranoe.com/builds/) (prendre de préférence le *build* statique, si possible en 64 bits).

Le code source se trouve aux chemins suivants :

\\\\pandore\\USBShare\\msis\\rjvb\\QuickTimeStuff\\brigade

\\\\pandore\\USBShare\\msis\\rjvb\\QuickTimeStuff\\QTImage2Mov

ftp://[anonymous@ftp.inrets.fr](mailto:anonymous@ftp.inrets.fr)/incoming/FtpSimU/logiciels/RJVB/brigade.tar.bz2

ftp://[anonymous@ftp.inrets.fr](mailto:anonymous@ftp.inrets.fr)/incoming/FtpSimU/logiciels/RJVB/QTImage2Mov.tar.bz2

ftp://[anonymous@ftp.inrets.fr](mailto:anonymous@ftp.inrets.fr)/incoming/FtpSimU/logiciels/RJVB/QuickTime-Installers/QuickTimeSDK-73.zip

\\\\pandore\\USBShare\\msis\\rjvb\\Libs

Pour la compilation, il faut installer le SDK de QuickTime à l'endroit proposé par défaut (C:\\Program Files) puis utiliser la solution Microsoft Visual Studio 2010 (Express), QTImage2MovVS2010.sln . La version 'développeur' utilise SS\_Log pour afficher des messages de suivi; le code nécessaire est attendu dans C:\\Libs .

**Mac OS X****:**

QTImage2Mov prend la forme d'un *bundle*: **QTImage2Mov.component**. Cet élément est à placer dans **/Library/QuickTime** ou dans le dossier **QuickTime** dans la **Bibliothèque** dans votre dossier de départ (**\~/Library/QuickTime**).

*FFMpeg* peut être installé via MacPorts ou Fink.

D'autres notes (plutôt concernant le développement) sont disponibles dans le fichier *TODOHIST* dans le répertoire du code source.

<sup>1</sup> cf. [http://developer.apple.com/library/mac/\#documentation/QuickTime/RM/MovieBasics/MTEditing/I-Chapter/9TimecodeMediaHandle.html](http://developer.apple.com/library/mac/#documentation/QuickTime/RM/MovieBasics/MTEditing/I-Chapter/9TimecodeMediaHandle.html)<sup>1</sup> Actuellement ignoré et bridé à l'équivalent de 1 trame de la vidéo.<sup>1</sup> À partir de la version 12, la piste TimeCode est cachée par défaut (et la piste timeStamp visible) en mettant *sa hauteur à 1 pixel*. Ceci permet de la rendre visible à nouveau, contrairement à un redimensionnement à 0 pixels.<sup>2</sup> Les pistes *timeStamp* sont spécifiques à la séquence importée et il peut donc y avoir plusieurs de ces pistes. Par contre, les données TimeCode de toutes les séquences importées sont enregistrées dans une seule piste TimeCode, et il ne peut donc y avoir plusieurs de ces pistes *que* quand on importe des séquences (.mov) entières qui en contiennent déjà.
