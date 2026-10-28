# GDD — Spooky_Spores

Document de conception complet du jeu : pourquoi chaque mécanique existe, comment elles s'articulent entre elles, et ce qui doit guider les décisions de level design et de contenu à venir.

Contrairement à [`documentation-technique.md`](documentation-technique.md), qui décrit le code et les systèmes **tels qu'ils existent**, ce document décrit **l'intention** derrière le jeu — la fiction, le monde, la progression du joueur, et les règles qui doivent trancher une décision de conception sans avoir à improviser à chaque fois.

**Statut** : en cours de rédaction, démarré le 2026-10-26. Point de passage obligatoire avant toute reprise de développement de mécanique (voir [`CLAUDE.md`](CLAUDE.md) section 12).

**Document complet et autonome** : ce GDD doit pouvoir se lire seul, sans avoir à ouvrir d'autres fichiers pour comprendre le jeu. Le 3C ([`README.md`](README.md)) et l'architecture technique actuelle ([`documentation-technique.md`](documentation-technique.md)) sont donc recopiés ici quand ils sont nécessaires à une section, même si l'information existe déjà ailleurs. Ces deux fichiers restent les sources canoniques de leurs propres domaines : en cas de changement d'une valeur (vitesse, FOV, implémentation...), il faut répercuter la mise à jour dans les documents concernés.

Non dupliqué : le backlog et le planning ([`TODO.md`](TODO.md)) — ce n'est pas de la conception, c'est du suivi de projet, qui n'a pas sa place ici.

---

## 1. Boucle de jeu et progression

### Le tutoriel comme première boucle complète

Le joueur apparaît dans la cabane abandonnée en exilé ordinaire, sans pouvoir — pas encore nécromancien. Une zone d'exploration courte et linéaire (rien d'autre à y faire, pas besoin de bloquer techniquement les systèmes de jeu) le mène jusqu'au sous-sol caché (voir section 2), où il découvre l'objet maudit. Cette découverte, racontée avec les outils déjà en place (`AStoryTrigger`/`WBP_StoryText`, du texte pour l'instant — pas une cinématique montée, qui reste du ressort du Bloc 3, voir `CLAUDE.md` section 2), le fait devenir nécromancien et débloque le tutoriel.

Une série de quêtes guide ensuite le joueur à travers un **premier passage complet de la boucle de jeu principale** : récolter une première ressource à la main, planter, préparer une première potion au chaudron, puis affronter la toute première vague d'ennemis (déclenchée à la fin du tutoriel), récupérer les os laissés au sol, ressusciter un premier serviteur, et lui assigner une tâche.

**Choix de conception — le tutoriel utilise les systèmes réels, pas une version dédiée** : la première vague, la première résurrection et le premier serviteur sont mis en scène par des marqueurs de quête sur les mécaniques génériques déjà prévues (vagues, résurrection, assignation de tâches), pas par un système parallèle « spécial tutoriel ». Objectif : un seul système à maintenir et équilibrer, cohérent avec le principe déjà appliqué dans le code (composition générique plutôt que système dédié à chaque cas, voir `documentation-technique.md` section 11).

### La boucle principale

récolter → planter → fabriquer (chaudron, et d'autres systèmes à venir) → combattre → ressusciter → assigner des tâches aux serviteurs → récolter.

Ce qui pousse le joueur à avancer à chaque étape n'est pas uniforme :
- **Récolte → plantation** : nécessité, pas incitation. Les ressources sauvages à proximité de la cabane s'épuisent avec le temps ; planter dans son propre territoire garantit un accès continu sans avoir à s'éloigner (et s'éloigner devient plus risqué à mesure que les vagues progressent).
- **Fabrication** : transforme les ressources brutes en progression concrète (potions, et d'autres usages à venir).
- **Combat** : obligatoire, pas un choix. Une vague d'ennemis détruit ce qu'elle atteint si le joueur ne la repousse pas (voir « enjeu et conditions d'échec » ci-dessous).
- **Résurrection → serviteurs** : nécessité de temps, pas curiosité. Un seul joueur ne peut pas à la fois défendre sa base et faire prospérer ses champs ; les serviteurs prennent en charge une partie du travail pour le libérer.

### Rythme : une run bornée, pas une partie infinie

La partie se joue en **une run d'environ 1h à 1h30**, pas une partie persistante infinie. Une run se termine par une victoire (voir l'objectif de fin, section 2) ou une défaite (destruction du noyau). Les deux issues débloquent des récompenses de méta-progression — cosmétiques pour le joueur et/ou la cabane, calculées sur le nombre de vagues survécues pendant la run — qui donnent envie de relancer une nouvelle run plutôt que de continuer la même indéfiniment.

**Choix de conception — pourquoi une run bornée plutôt qu'un bac à sable persistant** : évite le risque qu'une boucle qui ne fait que « grossir » (plus de champs, plus de serviteurs, vagues plus difficiles) finisse par lasser faute de nouveauté. Avec une durée connue à l'avance, la difficulté et les débloquages (nouveaux sorts, nouveaux bâtiments — file d'attente `TODO.md`) peuvent être calibrés sur un rythme précis, plutôt que de devoir inventer indéfiniment du contenu pour soutenir une progression sans fin.

### Déclenchement des vagues

La toute première vague attend que le joueur ait terminé le tutoriel. Ensuite, les vagues suivantes arrivent sur un **chrono régulier**, indépendamment de ce que fait le joueur — pas de seuil de progression à atteindre. La difficulté augmente d'abord en **quantité** d'ennemis, puis en **puissance**, à mesure que la run avance.

### Enjeu et conditions d'échec

Le joueur est **invincible** : les ennemis n'attaquent jamais le joueur lui-même, seulement les serviteurs et les bâtiments de sa base. Pendant une vague, le joueur agit uniquement via ses sorts, pour protéger ce qui peut être détruit.

- **Serviteurs** : peuvent être tués pendant une vague. Un serviteur tué disparaît définitivement — pas de restitution d'os à sa mort, pour éviter qu'un joueur puisse sacrifier des serviteurs à volonté afin d'en fabriquer d'autres sans coût réel.
- **Bâtiments secondaires** (champs, clôtures...) : leur destruction n'est qu'une perte temporaire, il faut les reconstruire. Un champ détruit redonne une graine au joueur, pour qu'il ne perde jamais définitivement l'accès à une ressource.
- **Le noyau** : objet unique du monde, lié à l'origine du joueur en tant que nécromancien (détail narratif : section 2). Sa destruction est la seule condition de défaite : la run s'arrête, le joueur recommence une nouvelle run depuis le début.

### Le recyclage des ressources (éviter les impasses)

Principe : une ressource ne doit jamais devenir totalement inutile une fois son besoin initial comblé — mais chaque ressource règle ce problème à sa manière, plutôt que via un système générique unique :
- **Bois / pierre** : la reconstruction des bâtiments détruits par les vagues est un puits de consommation continu — pas besoin de mécanique dédiée.
- **Graines** : à revérifier une fois l'inventaire limité en place (file d'attente, item 7) — un surplus de graines ne coûte rien au joueur tant que l'inventaire reste illimité.
- **Os** : les ennemis plus forts (plus rares) donnent des os de meilleure qualité, qui servent à ressusciter des serviteurs plus efficaces — c'est la voie principale. En complément, un surplus de petits os peut être converti vers l'équivalent d'un os de meilleure qualité, mais à un taux volontairement défavorable : cette conversion est un pis-aller pour qu'un stock de petits os ne devienne jamais un pur déchet, pas un substitut à la chasse aux ennemis forts (sinon la rareté des bons os perdrait tout son sens). Chaque conversion produit un **nouveau serviteur indépendant** (pas une amélioration rétroactive d'un serviteur existant). **Détail d'implémentation restant (lieu de la conversion — chaudron ou mécanique séparée) à trancher en section 4, voir dépendance avec l'UI de sélection de recette.**
  **Sacrifice volontaire d'un serviteur** : en complément, le joueur peut aussi sacrifier de son plein gré un serviteur existant (hors combat, à son initiative) contre un remboursement partiel en os — toujours inférieur au coût de création initial. Objectif : pouvoir se débarrasser d'un serviteur jugé trop faible sans que ce soit gratuit, sans pour autant être aussi punitif qu'une mort au combat (qui ne redonne jamais rien). Les deux règles coexistent volontairement, avec deux intentions différentes : empêcher un farming d'os sans coût (mort au combat) d'un côté, permettre un vrai choix de gestion de son armée à un coût réel (sacrifice volontaire) de l'autre.

---

## 2. Monde et factions

### Qui sont les villageois, et pourquoi ils attaquent

Des gens ordinaires d'un village voisin, sans organisation militaire particulière au départ. Ils remarquent la présence du joueur devenu nécromancien et la puissance qu'il accumule, et décident de l'arrêter avant qu'il ne devienne trop dangereux. Les premiers envoyés ne reviennent jamais ; à mesure que le temps passe sans nouvelles, le village envoie des groupes de plus en plus nombreux, puis de plus en plus puissants — ce qui justifie directement, dans la fiction, l'escalade mécanique des vagues (nombre d'abord, puissance ensuite, voir section 1). Plus la run avance, plus le village « se vide » de ses défenseurs, ce qui prépare aussi la logique du déblocage de la fin de partie (voir plus bas).

**Choix narratif — le joueur est un vrai antagoniste** : il ne pille pas les ressources du village, mais menace de tout détruire. Le village se défend légitimement ; ce n'est pas une agression irrationnelle de leur part. Conséquence à garder en tête pour la direction artistique (section 5) : la fin de partie ne doit pas être jouée comme une victoire purement triomphante.

### L'origine du joueur

Le joueur vivait autrefois dans ce village, avant d'en être exilé — la raison exacte n'est pas encore décidée, et rien n'indique qu'elle ait besoin de l'être : c'est ce que le joueur fait *après* son exil (lever une armée, menacer de tout détruire) qui justifie la réaction du village, pas l'exil en lui-même. Après son exil, il tombe sur une cabane abandonnée et s'y installe.

En explorant les environs, il découvre un sous-sol caché sous la cabane, où l'ancien occupant (voir ci-dessous) pratiquait secrètement la nécromancie. Il y trouve un objet maudit qui lui donne le pouvoir de devenir nécromancien à son tour. Habité par l'envie de vengeance contre le village qui l'a rejeté, il décide de lever une armée de serviteurs pour le conquérir.

**Le noyau** (défini en section 1 comme condition de défaite) est cet objet maudit, ou ce qu'il en reste une fois son pouvoir déployé — visuel exact à préciser plus tard, pas bloquant pour l'instant.

### La cabane et son passé

La cabane appartenait à un ancien moine du village, mort de vieillesse ; personne n'a voulu la récupérer après sa mort, et elle s'est progressivement dégradée. Le moine pratiquait lui-même la nécromancie en secret, dans un sous-sol caché sous la cabane — c'est là que le joueur trouve l'objet maudit. Ce lien explique pourquoi un objet aussi dangereux se trouvait justement à cet endroit, plutôt que n'importe où ailleurs.

**Choix technique — pas de refonte de la cabane existante** : l'extérieur de la cabane (déjà construit et fusionné, voir `documentation-technique.md` section 12) reste inchangé — une cabane modeste convient très bien à un moine vivant simplement. Le sous-sol est un **ajout**, pas une reconstruction : une pièce indépendante plutôt qu'une excavation du Landscape existant (creuser une vraie grotte dans un terrain déjà sculpté est une opération technique risquée, à éviter). La solution la plus simple : une petite pièce fermée construite avec la même technique que la cabane elle-même (géométrie simple, assets d'environnement achetés), reliée à l'extérieur par une trappe ou une porte — pas une vraie excavation du terrain. Détail d'implémentation exact à trancher quand cette tâche arrivera dans la file d'attente, pas maintenant.

### Ce que signifie « conquérir le village »

Le joueur n'atteint jamais physiquement le village — il n'y a pas de niveau d'assaut jouable. Dans la continuité du chemin qui descend de la colline (déjà prévu, voir `documentation-technique.md` section 12), une zone de fin devient accessible une fois le joueur suffisamment puissant (seuil exact — nombre de serviteurs ou de vagues survécues — à trancher en section 4). Le joueur reçoit un indicateur quand cette condition est remplie. En entrant dans cette zone, il déclenche un choix : terminer la partie maintenant, ou continuer à jouer normalement.

S'il choisit de terminer, une cinématique montre le joueur et son armée conquérir le village, et la run se termine sur une victoire — teintée par le fait que le joueur reste, de son plein gré, l'antagoniste de cette histoire (voir plus haut).

**Choix de conception — quelques variantes à seuils, pas une variation continue** : la cinématique varie selon la performance (nombre de serviteurs, vagues survécues), avec davantage d'alliés visibles et davantage de victimes montrées côté village pour une run plus aboutie — ça renforce le ton antagoniste posé plus haut. Mais plutôt qu'une population dynamique de la séquence pilotée en direct par les données de sauvegarde (lourd techniquement, et toujours du ressort du Bloc 3), on vise un **petit nombre de variantes distinctes** (ex : petite / moyenne / grande armée), choisies par un seuil.

**Conséquence sur la file d'attente** : les items 16 et 17 (`CLAUDE.md` section 12 — disposition du village + patrouilles, condition de victoire + clôture narrative) semblent largement simplifiés par cette décision : plus besoin de construire un village jouable ni des patrouilles de soldats, seulement une zone de déclenchement et une cinématique. À confirmer et à répercuter sur la file d'attente une fois la section 4 (mécaniques et dépendances) traitée.

---

## 3. Piliers de level design

3 à 4 règles simples qui doivent guider chaque espace construit, pour pouvoir juger une décision de level design sans avoir à repartir des principes à chaque fois.

### Le retour a un coût, pas l'éloignement en lui-même

Les ressources sont réparties de façon globalement uniforme sur toute la carte (hors chemin d'accès, voir pilier suivant) — pas de densité volontairement décroissante avec la distance à la base. Le risque ne vient donc pas d'une carte conçue pour punir l'éloignement, mais d'un mécanisme déjà posé en section 1 : les ressources sauvages proches de la cabane s'épuisent avec le temps, ce qui pousse naturellement le joueur à s'aventurer plus loin — où le vrai risque devient le temps nécessaire pour revenir défendre la base en cas de vague, pas une menace directe sur place.

**Conséquence concrète pour le placement** : un nœud de ressource peut être placé n'importe où sur la carte tant que ça reste cohérent avec l'environnement (pas d'arbre planté en plein milieu du chemin praticable) — y compris loin de la base. Pas de zone « riche » artificiellement réservée aux abords immédiats de la cabane.

### Le chemin d'accès reste toujours praticable

Il n'existe qu'un seul chemin reliant la cabane au village (celui déjà sculpté dans le Landscape, voir `documentation-technique.md` section 12), et ça restera le cas — pas de pluralité de fronts à défendre prévue. Ce chemin doit rester **impossible à bloquer entièrement** par une construction du joueur : le murer reviendrait à neutraliser toutes les vagues sans effort, ce qui viderait le combat de son enjeu.

**Choix de conception — système de construction en grille** : le futur système de construction (vue top-down, `TODO.md` items 14-15) reposera sur une grille de cases virtuelles, comme dans un city-builder. Les cases correspondant au chemin seront marquées non constructibles en permanence (affichées en rouge dans l'interface de placement) — pas de règle de « largeur minimale à préserver » à vérifier au cas par cas : la contrainte est déjà résolue par la grille elle-même, une case est soit constructible, soit non, sans dégradé possible. **Dépendance à reporter en section 4** : ce choix de grille doit être répercuté dans la description de l'item 14 (construction de base) quand cette section sera rédigée.

### Une colline, pas un monde ouvert

L'espace explorable reste borné à la colline et ses abords immédiats — assez pour une vraie variété de ressources et un aller-retour qui a un coût réel (pilier précédent), mais pas une carte ouverte étendue à explorer. Toute extension de l'espace construit doit être justifiée par un besoin mécanique concret (une ressource de plus à placer, par exemple), pas par du remplissage.

**Point ouvert, pas bloquant pour ce document** : une zone jouable bornée s'arrête quelque part — reste à décider comment habiller ce qui se trouve au-delà pour éviter un vide visuel (forêt dense formant un horizon naturel, brouillard de distance, relief qui ferme la vue...). Détail à trancher au moment de construire concrètement ces abords, pas maintenant.

---

## 4. Mécaniques et leurs dépendances

Liste des mécaniques déjà en place (voir `CLAUDE.md` section 7) et à venir (section 12), et comment elles dépendent les unes des autres.

*(à rédiger)*

---

## 5. Direction artistique minimale

Ton, ambiance, références. Assez pour trancher si un choix (asset, couleur, son) colle au jeu ou pas, sans y repenser à chaque fois.

*(à rédiger)*
