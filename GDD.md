# GDD — Spooky_Spores

Document de conception complet du jeu : pourquoi chaque mécanique existe, comment elles s'articulent entre elles, et ce qui doit guider les décisions de level design et de contenu à venir.

Contrairement à [`documentation-technique.md`](documentation-technique.md), qui décrit le code et les systèmes **tels qu'ils existent**, ce document décrit **l'intention** derrière le jeu — la fiction, le monde, la progression du joueur, et les règles qui doivent trancher une décision de conception sans avoir à improviser à chaque fois.

**Statut** : en cours de rédaction, démarré le 2026-10-26. Point de passage obligatoire avant toute reprise de développement de mécanique (voir [`CLAUDE.md`](CLAUDE.md) section 12).

**Document complet et autonome** : ce GDD doit pouvoir se lire seul, sans avoir à ouvrir d'autres fichiers pour comprendre le jeu. Le 3C ([`README.md`](README.md)) et l'architecture technique actuelle ([`documentation-technique.md`](documentation-technique.md)) sont donc recopiés ici quand ils sont nécessaires à une section, même si l'information existe déjà ailleurs. Ces deux fichiers restent les sources canoniques de leurs propres domaines : en cas de changement d'une valeur (vitesse, FOV, implémentation...), il faut répercuter la mise à jour dans les documents concernés.

Non dupliqué : le backlog et le planning ([`TODO.md`](TODO.md)) — ce n'est pas de la conception, c'est du suivi de projet, qui n'a pas sa place ici.

---

## 1. Boucle de jeu et progression

### Le tutoriel comme première boucle complète

Le joueur apparaît dans la cabane abandonnée sans contexte. Une introduction narrative (texte ou cinématique courte, réutilisant `AStoryTrigger`/`WBP_StoryText` déjà en place) explique la situation : qui il est, où il est, pourquoi il est là (détail du récit : voir section 2). Une série de quêtes guide ensuite le joueur à travers un **premier passage complet de la boucle de jeu principale** : récolter une première ressource à la main, planter, préparer une première potion au chaudron, puis affronter la toute première vague d'ennemis (déclenchée à la fin du tutoriel), récupérer les os laissés au sol, ressusciter un premier serviteur, et lui assigner une tâche.

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

Qui sont les villageois ? Pourquoi envoient-ils des soldats contre le joueur ? Que signifie concrètement « conquérir le village » en fin de jeu ? Quelle est l'histoire du joueur et de sa cabane ?

*(à rédiger)*

---

## 3. Piliers de level design

3 à 4 règles simples qui doivent guider chaque espace construit, pour pouvoir juger une décision de level design sans avoir à repartir des principes à chaque fois.

*(à rédiger)*

---

## 4. Mécaniques et leurs dépendances

Liste des mécaniques déjà en place (voir `CLAUDE.md` section 7) et à venir (section 12), et comment elles dépendent les unes des autres.

*(à rédiger)*

---

## 5. Direction artistique minimale

Ton, ambiance, références. Assez pour trancher si un choix (asset, couleur, son) colle au jeu ou pas, sans y repenser à chaque fois.

*(à rédiger)*
