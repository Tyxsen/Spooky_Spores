# Spooky_Spores

Un jeu bac à sable en Unreal Engine dans lequel le joueur incarne un nécromancien : explorer, récolter, manipuler l'environnement par la magie, faire prospérer une base autonome, et défendre son territoire contre les habitants du village voisin.

> Projet réalisé dans le cadre du Master *Réalisation de Jeux Vidéo, Rendu Temps Réel 3D et Technologies Immersives* — Gaming Campus (GTech 4).

---

## 📥 Builds

Les builds jouables du projet sont disponibles dans la section **[Releases](../../releases)** de ce dépôt GitHub.

---

## 🎮 Description

Le joueur est un nécromancien qui apparaît dans une petite cabane abandonnée. Son but est de récolter les ressources aux alentours de sa cabane pour développer sa base en ajoutant par exemple des champs, automatisés grâce à un système de serviteurs. Grâce aux récoltes de ces champs, le joueur peut augmenter la puissance et/ou la rapidité d'exécution de ses troupes.

Une fois sa base bien développé, le joueur finit par se faire remarquer par les PNJ du village voisin, qui envoient des soldats pour le combattre. Lors de ces attaques, le joueur doit défendre sa base pour éviter qu'elle soit détruite mais également de récupérer les ressources laissées par les PNJ (boucliers, piques...) pour solidifier ses constructions.

Avec les squelettes de ses ennemis, le joueur peut, sous certaines conditions, les ressusciter ou les transformer en morts-vivants pour en faire des serviteurs. Des potions, créées à partir de ressources récoltées et préparées dans un chaudron, permettent de transformer des ennemis en serviteurs, d'améliorer la vitesse de pousse des ressources, l'efficacité des serviteurs, ou leur puissance.

Le joueur explore donc les environs pour récolter des plantations, des roches, ou toute autre ressource utile à l'amélioration de sa base, de ses serviteurs, de ses champs, ou à la création de potions.

**Quête optionnelle de fin de jeu :** rassembler les troupes accumulées pour aller conquérir le village qui envoie des soldats tout au long du jeu, et en faire son propre territoire.

---

## 🧍 3C — Character, Camera, Controls

### Character

Le personnage est de taille moyenne (environ 1m80). Il peut se déplacer, courir pour aller plus vite, interagir avec plusieurs types d'objets ou d'environnements (champs, établis d'alchimie, ressources de l'environnement, ennemis morts pour récupérer leurs armes ou leurs os), et lancer des sorts pour attaquer les soldats ennemis venant assaillir la base.

**Déplacement**
- Vitesse de marche : **400 unités**
- Vitesse de course : **650 unités**
- Endurance : suffisante pour courir une dizaine de secondes, se recharge après 4 secondes sans courir

**Interaction**
- Portée d'interaction avec l'environnement : **2 à 3 mètres**, depuis la tête du personnage
- Objet grab : Peut être lancé avec une impulsion définie (1000)

**Combat**
- Sorts infligeant des dégâts modérés, proportionnels au niveau du sort
- Variété de sorts envisagée : aveuglement (réduit la précision ou empêche le tir des archers), gel (ralentit les ennemis), feu (dégâts sur la durée)
- Coût en mana avec recharge automatique (~5 secondes pour une attaque complète)
- 4 utilisations de sorts disponibles au maximum

### Camera

Caméra en **première personne**, choisie pour maximiser l'immersion du joueur dans l'univers et le personnage. Positionnée à hauteur d'un humain d'1m80, elle facilite les interactions avec les champs, les établis, et l'environnement en général (le joueur interagit en regardant un objet et en appuyant sur une touche).

- FOV par défaut : **90°**, réglable à tout moment dans les paramètres du jeu

### Controls

| Action | Touche |
|---|---|
| Déplacement | Z Q S D |
| Regarder autour de soi | Souris |
| Ouvrir l'inventaire | Tab |
| Interagir avec l'environnement / les objets | E |
| Attaquer (sort sélectionné) | Clic gauche |
| Lancer un objet actuellement grab | Clic droit |
| Sélectionner un sort (roue des sorts) | Maintenir clic droit puis relâcher sur le sort voulu |
| Barre de raccourcis (9 emplacements) | 1 à 9 (par défaut, remappable) |
| Basculer vue joueur ↔ vue de gestion (top-down) | Touche à définir |

Les réglages (son, contrôles, FOV) sont accessibles et modifiables depuis le menu des paramètres.

### Gestion de la base

Une touche dédiée permet de basculer de la vue première personne par défaut vers une vue de dessus (top-down), pensée pour la gestion des serviteurs, de la base, des champs, et de tout ce qui s'y rattache.

---

## 🛠️ Moteur

Unreal Engine **5.8.2**
