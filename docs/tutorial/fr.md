# WhiteHoleFX – Tutoriel

Ce tutoriel vous mène en une demi-heure environ du premier démarrage à votre propre effet sur vos LED. Le même tutoriel existe
en [allemand](de.md), en [anglais](en.md) et en [espagnol](es.md). Dans le programme, vous le trouvez sous **Aide → Tutoriel**,
dans la langue choisie sous **Aide → Sprache / Language**.

## 1. Ce que fait WhiteHoleFX

WhiteHoleFX pilote des matrices LED, des strips, des cubes et toute disposition de pixels. Vous construisez un effet à partir de
**nœuds** (blocs comme dégradé, plasma, rotation, miroir) reliés par des câbles. L'effet est calculé sur la carte graphique,
échantillonné pour chaque LED à sa position dans l'espace et envoyé à vos contrôleurs par **Art-Net**, **sACN** ou **DDP**.

Trois termes sont utiles :

- **Fixture :** un élément matériel avec des LED, par exemple une matrice, un strip ou une grille 3D (cube).
- **Patch :** la destination des données d'un fixture (adresse du contrôleur, univers, canal, ordre des couleurs).
- **Look :** un effet complet enregistré, que vous rappelez d'un clic en mode live.

## 2. Premier démarrage

1. Lancez `whitehole_studio`.
2. Ouvrez un exemple : **Fichier → Exemples → Premiers pas → wand**. L'exemple se compose d'une matrice de 16×8 LED et d'un strip.
3. Appuyez sur la **barre d'espace** (démarrer). L'aperçu au centre montre l'effet en cours.

**Important :** après chaque démarrage, la sortie réseau est **désactivée**. Le projet tourne et s'affiche dans l'aperçu, mais
aucun paquet ne quitte votre ordinateur. Seul **Lecture → Envoyer la sortie** envoie vraiment des données aux contrôleurs ; le
bouton devient alors rouge. Ainsi, rien ne peut s'allumer par erreur sur une installation étrangère.

## 3. Installer la scène (Stage Config)

Quand on débute, on commence par le matériel. Ouvrez **Stage Config → Disposition et patch …** (F4). La fenêtre séparée comporte
ces pages :

- **Fixtures :** la liste de tous les fixtures. **Ajouter un fixture …** ouvre une boîte de dialogue pour saisir le type
  (matrice, strip, grille 3D), la taille en x, y et z, l'écart, la position dans l'espace et le câblage : le coin de départ, si
  le câble suit un serpentin et quel axe compte le plus vite. La petite vue 3D en haut montre l'effet de chaque valeur. La
  première LED du câble est **blanche**, ce qui permet de reconnaître le coin de départ.
- **Destinations (contrôleurs) :** les adresses de vos contrôleurs. Une adresse vide signifie multicast.
- **Mode test :** envoie des mires (couleur, barre de balayage, ordre des couleurs, chenillard) directement aux LED pour vérifier
  câblage et patch sans effet en cours. **Attention :** le blanc s'allume à pleine luminosité ; vérifiez d'abord les alimentations.
- **Vue :** l'installation dans l'espace, orientable, avec une couleur par fixture et un repère d'axes (x rouge, y vert, z bleu).

Un cube se crée comme **grille 3D**, par exemple 8 × 8 × 8 LED. Pour des strips enroulés autour d'une colonne, utilisez un
**nuage de points** : **Stage Config → Importer un nuage de points …** lit les positions des LED dans un fichier CSV ou JSON.

Astuce : ouvrez **Fichier → Exemples → 3D → ausrichtung**. Chaque LED montre sa position sous forme de couleur (rouge = x, vert =
y, bleu = z). Si l'image ne correspond pas à votre montage, modifiez le coin de départ et l'ordre dans le fixture.

## 4. Construire un effet (éditeur de nœuds)

En bas de la fenêtre se trouve le panneau **Nœuds** : la **palette** à gauche, l'**éditeur** à droite.

- **Ajouter un nœud :** le glisser de la palette vers la zone, double-cliquer, clic droit sur la zone vide, ou **Ctrl+Espace** puis
  taper (« rain », « mix », « blend add »).
- **Relier :** glisser d'une sortie (pleine, à droite) vers une entrée (anneau, à gauche). La couleur et la forme d'un port
  indiquent son type, seuls des ports compatibles se relient. Une entrée occupée est remplacée.
- **Choisir la sortie :** clic droit sur un nœud → **Définir comme sortie (output)**. Ce nœud décide de ce qui apparaît sur les LED
  (contour orange).
- **Supprimer :** cliquer sur un nœud, **Suppr**. S'il se trouve dans une chaîne (A → X → B), l'éditeur relie A **directement à B** ;
  la chaîne reste donc intacte. **Ctrl+Z** annule tout.
- **Si une entrée manque,** les LED restent **éteintes** (la ligne d'état indique « ENTRÉE MANQUANTE ») au lieu de figer la
  dernière image. Dès que le graphe est de nouveau complet, l'effet repart.

**Favoris :** les catégories de la palette sont repliées au démarrage. Un clic droit sur un type de nœud l'ajoute au groupe
**Favoris**, qui reste en haut et est mémorisé. Pendant une recherche, les résultats sont dépliés.

Un petit exemple : glissez **Linear Gradient** et **Rotate** sur la zone, reliez `out` du dégradé à `source` de Rotate, définissez
Rotate comme sortie. Ajoutez ensuite **Time** et reliez `seconds` à l'entrée `angle` de Rotate : le dégradé tourne.

## 5. Régler les paramètres

Dans le panneau **Paramètres** (à droite), chaque nœud a ses réglages : nombres, listes de choix, interrupteurs oui/non et
couleurs. Un clic sur un nœud dans l'éditeur affiche ses paramètres. Les réglages reliés à quelque chose (par exemple un LFO)
sont grisés. Le symbole ↺ rétablit la valeur par défaut. Sous **Édition → Réglages → Valeurs par défaut des nœuds**, vous
choisissez avec quelles valeurs démarrent les nouveaux nœuds.

## 6. La 3D

Tous les générateurs connaissent la profondeur (z). En 2D (une matrice), cela ne change rien ; dans un cube, l'effet remplit
l'espace. Essayez les cinq exemples 3D sous **Fichier → Exemples → 3D** :

- **ausrichtung :** vérifie les axes et le câblage (Position Test).
- **plasma-wuerfel :** un plasma qui traverse la profondeur, avec dégradé de couleurs et rotation.
- **scheibe :** une tranche traverse le cube.
- **regentropfen :** des gouttes en coques sphériques dans l'espace.
- **saeule :** un strip enroulé en hélice autour d'une colonne.

La barre **Affichage** bascule entre **2D (éditer)** et les vues 3D (libre, face, côté, dessus). En 3D, on tourne en faisant
glisser, le clic droit déplace, la molette zoome. Sous **Affichage → Affichage 3D**, vous masquez les LED éteintes (par défaut,
pour voir à travers le cube) et activez le halo additif.

## 7. Looks et mode live

Un effet qui vous plaît s'enregistre avec **Ctrl+L** comme **look** (il apparaît dans le panneau **Looks** et dans le fichier du
projet). **F9** lance le **mode live** : l'éditeur disparaît et chaque look reçoit un grand bouton. Un clic (ou la touche 1 à 9)
lance le fondu dans la durée réglée. Avec « Avec GO », vous ne faites que précharger un look et lancez le fondu avec la **barre
d'espace**. **Échap** quitte le mode live. **Master** règle la luminosité globale, **Ctrl+B** est le blackout.

## 8. Audio et DMX en entrée

Sous **Édition → Réglages**, vous choisissez la carte réseau, le périphérique audio et l'entrée DMX. Les nœuds audio (niveau, bande
de fréquences, beat, spectre, oscilloscope) réagissent alors à la musique ; les nœuds DMX lisent des valeurs d'une console
lumière. Des exemples se trouvent sous **Fichier → Exemples → Audio**.

## 9. Changer de langue

**Aide → Sprache / Language** propose Deutsch, English, Français et Español. Le choix est mémorisé ; si vous le souhaitez, le
programme redémarre aussitôt et rouvre votre projet. Les noms des nœuds et des paramètres restent en anglais pour que les guides
et les projets de toutes les langues soient compatibles.

## 10. En cas de problème

- **Pas de lumière sur les LED :** « Envoyer la sortie » est-il activé ? L'adresse du contrôleur sous Destinations est-elle correcte ?
  Le mode test aide-t-il ?
- **Mauvaises couleurs :** changez l'ordre des couleurs dans le patch (RGB, GRB …) ou lancez « Ordre des couleurs » en mode test.
- **Image à l'envers ou en miroir :** modifiez le coin de départ et les options `flip` du fixture.
- **Saccades :** réduisez la cadence ou la taille de la fenêtre ; pour les changements en live, préchargez avec « Avec GO ».
- **Autre chose :** **Aide → Signaler un bogue ou proposer une idée …** ouvre GitHub.

Pour aller plus loin, lisez les documents détaillés du dossier `docs/` : interface, fichier de projet, nœuds de champ, mapping,
sortie, audio et graphe de valeurs.
