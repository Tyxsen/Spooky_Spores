# Checklist d'intégration éditeur : zone de base (item 2.2b)

Spec : `docs/specs/base-zone.md`. Branche : `feature/base-zone`.
C++ concerné : `Source/Spooky_Spores/Base/` (`ABaseZone`, `UBaseZoneSubsystem`).

## 0. État de l'éditeur lors de la rédaction (2026-11-10)

- `ue_status` : **aucun éditeur joignable** (`connected: false`). Rien n'a été lu dans l'éditeur.
- Conséquence : les repères de `BP_Fences` (position, dimensions) ne sont **pas connus**. L'étape 5 les fait relever à l'exécution.
- Le Content Browser a été lu **sur le disque** (`Content/Spooky_Spores/Placeables/`) : `BP_Fences.uasset` est à la **racine** de `Placeables/` ; il existe `Destructibles/`, `Enemies/`, `Interactables/`, `Narrative/`, `Resources/`, `Servants/`. **Il n'existe pas de dossier `Base/`** (créé à l'étape 1, comme prévu par la spec section 7). `documentation-technique.md` section 2 n'est pas encore à jour sur ce point (mise à jour par `consistency-keeper` en fin de branche).
- Niveau : `Content/Spooky_Spores/Maps/Lvl_Principal.umap` existe.

## 1. Pré-requis

- Branche : `feature/base-zone` (jamais `dev` ni `main`), arbre de travail propre.
- **Arrêt obligatoire n°2 (compilation, par l'humain)** : voir section 3. Aucune étape [MCP] avant.
- Classes qui doivent exister dans l'éditeur après compilation :
  - `ABaseZone` (chemin probable `/Script/Spooky_Spores.BaseZone`, **à confirmer** avec `ue_search_assets` / `ue_asset_info` : éditeur éteint, non vérifiable)
  - `UBaseZoneSubsystem` : aucune étape d'intégration (sous-système automatique), mais sa présence valide la compilation.
- Assets déjà nécessaires : `BP_Fences` (existant, **non modifié**) ; `Lvl_Principal` ouvert dans l'éditeur par l'humain (`ue_open_level` est exclu).
- Une seule `BP_BaseZone` dans le niveau (décision 4). Une seconde instance est une erreur de level design (log `Error`, critère S3).

## 2. Étapes

Convention : [MCP] = exécutable par `bp-integrator` ; [HUMAIN] = tout le reste. Dans le doute, [HUMAIN].

### Étape 1 [MCP] : créer le dossier `Base`
- Action : créer `/Game/Spooky_Spores/Placeables/Base`.
- Raison : nouveau domaine, comme `Base/` côté C++ ; `BP_BaseZone` n'est ni une ressource ni un interactable (`Resources/` ou `Interactables/` seraient trompeurs).
- Résultat attendu : le dossier existe (`ue_search_assets` avec `root=/Game/Spooky_Spores/Placeables`).

### Étape 2 [MCP] : créer `BP_BaseZone`
- Action : créer un Blueprint enfant de la classe C++ `ABaseZone` à `/Game/Spooky_Spores/Placeables/Base/BP_BaseZone`. **Aucune logique, aucun Event Graph.**
- Résultat attendu : `ue_asset_info` sur ce chemin indique un Blueprint de classe parente `BaseZone`. Si la classe parente est introuvable : arrêt, la compilation n'a pas été faite.

### Étape 3 [HUMAIN] : régler `Hidden In Game` et `Visible` sur le composant `ZoneBox` de la classe `BP_BaseZone`
- Pourquoi [HUMAIN] : `ZoneBox` est un composant **créé dans le constructeur C++** et hérité. Je ne peux pas confirmer que `ue_set_blueprint_default` cible la propriété d'un composant hérité (schéma de l'outil non vu, éditeur éteint). Dans le doute : [HUMAIN]. Si un essai montre que l'outil le permet, l'étape pourra passer en [MCP].
- Action : ouvrir `BP_BaseZone`, onglet Components, sélectionner `ZoneBox` (la racine), panneau Details :
  - Rendering > **Hidden in Game** : **décoché** (`bHiddenInGame = false`).
  - Rendering > **Visible** : **coché** (`bVisible = true` ; défaut moteur, à vérifier plutôt que supposer).
  - Compiler et enregistrer le Blueprint.
- Pourquoi sur la **classe** et pas sur l'instance (spec 2.6) : le C++ garde `bHiddenInGame = true` (défaut moteur, sûr pour un build livré) ; le réglage de développement est une case à recocher avant Release, et ne subit pas le piège de l'instance surchargée (`CLAUDE.md` section 11).
- Résultat attendu : dans `ZoneBox` de `BP_BaseZone`, `Hidden in Game` décoché, `Visible` coché.

### Étape 4 [HUMAIN] : vérifier les valeurs par défaut de `BP_BaseZone`
- Pourquoi [HUMAIN] : même raison que l'étape 3 (lecture d'une propriété de composant hérité). À faire pendant que le Blueprint est ouvert.
- Vérifier, sans modifier :
  - `ZoneBox` > Shape > **Box Extent** = **(500, 500, 200)** (défaut du constructeur, spec section 5).
  - `ZoneBox` > Collision > **Collision Presets** = `NoCollision` ; **Generate Overlap Events** décoché ; **Can Ever Affect Navigation** décoché (spec 2.4, critère C1). La spec note que le nom `NoCollision` du preset intégré est à confirmer ; si l'éditeur affiche « Custom », le signaler.
  - `bIgnoreHeight` (catégorie `BaseZone`) = **coché**.
  - Tick : Class Defaults > Actor Tick > Start with Tick Enabled décoché (le C++ met `bCanEverTick = false`).
- Résultat attendu : valeurs conformes. Toute divergence est un écart à signaler, pas à corriger en silence.

### Étape 5 [MCP] : relever les repères de `BP_Fences` dans le niveau (lecture seule)
- Action : `ue_list_actors` avec `name_contains="Fences"`, puis `ue_get_actor` sur chaque résultat (le label exact est **inconnu** : le lire, ne pas le deviner). Relever position, rotation, échelle et liste de composants.
- Résultat attendu : le rapport de `bp-integrator` donne position/rotation/échelle de chaque acteur de clôture. Aucun outil ne donne la boîte englobante d'un acteur : si l'enceinte est faite de plusieurs segments ou d'un seul acteur multi-meshes, les dimensions ne sont peut-être pas lisibles ; l'étape 6 utilise alors un centre estimé (moyenne des positions) et l'étape 7 fait le calage.

### Étape 6 [MCP] : placer UNE instance de `BP_BaseZone` dans `Lvl_Principal`
- Pré-conditions : `Lvl_Principal` est le niveau courant (`ue_project_info`), sinon arrêt ; aucune instance de `BaseZone` n'existe déjà (`ue_list_actors`), sinon arrêt.
- Action : `ue_spawn_actor` de la classe générée par `BP_BaseZone` (chemin de classe attendu par l'outil : **à confirmer à l'exécution**, probablement `/Game/Spooky_Spores/Placeables/Base/BP_BaseZone.BP_BaseZone_C`, ne pas supposer), au centre estimé à l'étape 5 (X, Y de l'enceinte ; Z du sol de la clôture), rotation (0,0,0) ou le yaw de `BP_Fences` si l'enceinte est tournée, **échelle (1,1,1)**. Label `BaseZone` seulement si l'outil le permet.
- Choix : ne pas dimensionner par l'échelle de l'acteur ; l'étape 7 règle `Box Extent` (se lit directement ; le C++ gère les deux).
- Résultat attendu : `ue_list_actors` (filtre sur la classe) renvoie **exactement un** acteur ; `ue_get_actor` donne la position demandée, l'échelle (1,1,1) et un composant `ZoneBox`.
- Effet de bord attendu : One File Per Actor crée un fichier sous `Content/__ExternalActors__/` ; **`Lvl_Principal.umap` (point chaud) peut aussi être modifié** : à signaler dans le rapport.

### Étape 7 [HUMAIN] : caler la boîte sur l'intérieur de `BP_Fences`
- Pourquoi [HUMAIN] : le calage se juge à l'œil, et la propriété (`Box Extent` du composant `ZoneBox` d'une **instance**) est une propriété de composant hérité (même doute que l'étape 3). Les clôtures étant arrondies, une boîte approximative est acceptée (décision 3) : « ce qui est à l'intérieur des clôtures est dans la boîte ».
- Action : dans le viewport de `Lvl_Principal`, sélectionner l'instance, composant `ZoneBox`, Details :
  - ajuster **Box Extent** (**demi**-dimensions : 500 donne 10 m de côté) pour que la boîte tienne à l'intérieur des clôtures ;
  - ajuster au besoin position et yaw de l'acteur pour centrer la boîte ;
  - **ne pas toucher à `Hidden in Game` sur l'instance** (étape 8) ; ne pas utiliser l'échelle.
- Rappel (`CLAUDE.md` section 11) : une extension réglée à la main sur l'instance ignore ensuite tout changement de défaut du C++ ou du Blueprint, jusqu'à `Reset to Default`. Voulu pour l'extension, piège pour `Hidden in Game`.
- Résultat attendu : la boîte englobe l'intérieur des clôtures sans déborder largement. Relever dans le rapport l'extension et la transformation finales. Contrôle visuel complet en PIE (T1).

### Étape 8 [HUMAIN] : vérifier que l'instance ne surcharge pas `Hidden In Game` (critère G2)
- Pourquoi [HUMAIN] : lecture d'une propriété de composant hérité sur une instance ; rien ne garantit que `ue_get_actor` l'expose.
- Action : sélectionner l'instance, `ZoneBox` > Rendering : `Hidden in Game` **décoché** et **sans pastille de réinitialisation** (valeur héritée de `BP_BaseZone`). Même contrôle pour `Visible`. Si surchargé : `Reset to Default`, puis revérifier.
- Résultat attendu : valeur héritée, non surchargée.

### Étape 9 [MCP] : enregistrer
- Action : `ue_save_all`.
- Remarque : `ue_save_all` enregistre **tous** les assets modifiés. `bp-integrator` ne stage que les fichiers de la checklist : `BP_BaseZone.uasset`, le fichier d'acteur externe de l'étape 6, et `Lvl_Principal.umap` seulement s'il est modifié **et** accepté par l'orchestrateur (point chaud : à signaler, pas à stager en silence). Le reste est listé non stagé dans le rapport.
- Ordre conseillé : `bp-integrator` fait 1, 2, 5, 6 ; l'humain fait 3, 4, 7, 8 puis enregistre (Save All) ; `bp-integrator` (ou l'humain) refait 9 si besoin. Un seul acteur à la fois dans l'éditeur.
- Résultat attendu : `git status` montre `BP_BaseZone.uasset` et un fichier sous `__ExternalActors__/` ; aucun autre `.uasset` modifié par erreur.

## 3. Compilation et rechargement (humain, avant toute étape [MCP])

Le C++ de `Base/` est neuf (nouvelles `UCLASS`) : le Live Coding ne suffit pas.

1. **Fermer l'éditeur Unreal.**
2. Vérifier `git branch --show-current` = `feature/base-zone`.
3. Clic droit sur le `.uproject` > Generate project files (nouveaux fichiers C++, sinon Rider n'affiche pas `Base/`).
4. Rider : **Rebuild complet**.
5. **Rouvrir l'éditeur** sur `Lvl_Principal`.
6. Contrôle rapide : nouveau Blueprint > classe parente `BaseZone` trouvable.
7. Lancer alors `bp-integrator` (un seul à la fois ; l'humain ne touche pas à l'éditeur pendant son passage).

## 4. Scénario de test humain (PIE)

### T0 : Automation Specs
- Tools > Test Automation, filtre `SpookySpores.Base`, tout cocher, Start Tests. Attendu : tout vert. Si la liste est vide ou si un test manque, c'est un échec (nom exact du filtre à ajuster selon `qa-test-writer`).

### T1 : critère G1 (boîte filaire visible)
1. PIE depuis `Lvl_Principal`. Attendu : boîte filaire visible, posée sur l'intérieur des clôtures. Faire le tour pour juger le calage ; retoucher (étape 7) si besoin.
2. Arrêter le PIE, recocher `Hidden in Game` sur `ZoneBox` de `BP_BaseZone`, compiler, relancer : la boîte **disparaît**. Redécocher, compiler, enregistrer (état final voulu en développement : décoché).
3. Si la boîte ne s'affiche jamais décochée : limite non vérifiée de la spec 2.6 (`IsShown` non lu en entier). Aucun repli prévu : l'humain investigue.
4. `show collision` ne dessine **pas** cette boîte (preset `NoCollision`) : ce n'est pas un bug.

### T2 : log
- Output Log en PIE : aucune ligne `Error` ni `Warning` de `LogSpooky_Spores` mentionnant `BaseZone` (une seule zone, valide). « is invalid » = extension ou échelle nulle.

### T3 : le joueur n'est pas bloqué
- Traverser la zone de part en part, y compris en sprint et autour de la cabane : aucun blocage ni ralentissement.

### T4 : le trace d'interaction n'est pas gêné
- Dans la zone, viser rocher, plante récoltable, chaudron, parcelle : l'invite d'interaction apparaît normalement et ne cible jamais `BaseZone`. Viser un objet à travers la zone depuis l'extérieur : même comportement que sur `dev`.

### T5 : unicité
- Outliner, filtre `BaseZone` : un seul acteur.

## 5. Non-régression (à rejouer en PIE)

La zone est `NoCollision`, sans tick, sans appel depuis un système existant : risque faible, mais le placement modifie `Lvl_Principal`.

- Interaction (E) sur tous les types d'interactables du niveau.
- Grab (E), relâcher (E), lancer (clic droit) : un objet tenu ou lancé traverse la zone sans effet ; retombe normalement.
- Récolte (plante, pierre, bois) : drops qui apparaissent, rebondissent, se ramassent.
- Plantation (`BP_FieldPlot`) et chaudron (`BP_Cauldron`).
- **Pose d'une Necrospore (L) à l'intérieur ET à l'extérieur de la zone** : le serviteur apparaît dans les deux cas. Son comportement reste celui de 2.2a (l'errance dans la zone est l'item 2.2c, **non attendue ici**).
- Sprint (Maj gauche), saut, endurance.
- Sort (clic gauche) et mana.
- Ennemi `BP_EnemySoldier` et son parcours : pas de perturbation. `Show > Navigation` : le NavMesh n'est pas découpé par la zone.
- Panneau de contrôles (Entrée).

## 6. Après la validation

- La boîte reste visible pendant le développement. **Avant Release** : `Hidden in Game` à `true` sur `ZoneBox` de `BP_BaseZone` (ligne de dette technique proposée par la spec 2.6, écrite dans `TODO.md` par `consistency-keeper`).
- `documentation-technique.md` (section 2, dossier `Placeables/Base/`) : `consistency-keeper` uniquement.

## 7. Doutes et points non confirmés

- Éditeur éteint : aucune lecture réelle. Repères de `BP_Fences` inconnus.
- Schémas des outils `ue_set_blueprint_default`, `ue_set_actor_property`, `ue_spawn_actor` non vus : je ne sais pas s'ils ciblent un **composant hérité du C++** (`ZoneBox`). D'où les étapes 3, 4, 7, 8 en [HUMAIN]. Chemin de classe pour `ue_spawn_actor` à confirmer.
- Chemin `/Script/Spooky_Spores.BaseZone` à confirmer.
- Preset `NoCollision` : nom standard, à confirmer.
- G1 est strictement humain (`ue_play` exclu).
- Challenge : seules quatre étapes sont réellement [MCP] (1, 2, 5, 6) plus la sauvegarde, parce que l'essentiel du réglage porte sur `ZoneBox`, composant hérité. Si `ue_set_blueprint_default` accepte un chemin de sous-objet, un essai sur `BP_BaseZone` pourrait automatiser 3 et 4 : à tester une fois par l'orchestrateur, pas à supposer.
