# TODO — Spooky_Spores
 
## Prochaine étape
 
- [x] Panneau de contrôles en lecture seule (affichage des touches actuelles, toggle Entrée, pas de remappage pour l'instant) — terminé, testé, fusionné dans `dev`
- [x] Composant de santé + frapper les arbres et pierres + premier sort avec mana — terminé et testé (non-régression comprise), sur `feature/health-and-combat`, prêt à fusionner dans `dev`
- [x] Semaine de cours S2 : environnement du joueur (colline, cabane) livré, Release `V1.0.1` — fusionné dans `dev`
- [x] GDD complet (`GDD.md`, 5 sections) — rédigé du 2026-10-26 au 2026-11-03. La file d'attente ci-dessous a été réorganisée en conséquence (items 1, 2, 3, 13, 14, 16, 17 mis à jour). Le point de passage obligatoire (`CLAUDE.md` section 12) est levé.
- [x] Item 1 (soldat ennemi simple) : terminé, testé en PIE (suivi de spline, détection/approche du destructible le plus proche, attaque, mort, drop d'os via `AResourceDrop`), fusionné dans `dev` (PR #7)
- [x] Corriger les includes courts de `Enemies/EnemyAIController.cpp` et `Resources/ResourceCounterComponent.h`
- [x] Item 2, étape 2.1 : chaudron multi-recettes + recette os + potion → Necrospore (ressource comptée provisoire) — terminé et testé sur `feature/cauldron-recipes`
- [x] Item 2, étape 2.2a : pose de la Necrospore (touche L) → apparition d'un serviteur immobile — terminé et testé sur `feature/servant-placement`
- [ ] Item 2, étape 2.2b : zone de base (`UBoxComponent`, « point dans la zone ? » / « point au hasard dans la zone », avec debug visuel)

## Divers à régler
 
- [ ] `CLAUDE.md` : préciser quels projets de l'année doivent être rendus en anglais (dépend des CDC pas encore communiqués)
- [x] Supprimer la branche `feature/plantation` sur `origin` (déjà fusionnée dans `dev`)

## Planning

Depuis le 2026-11-09 : **plus de découpage par semaines**. On avance à notre rythme dans la file ci-dessous ; Timéo signale lui-même chaque deadline (rendu, oral) quand elle se présente, et on recale la file à ce moment-là. Règles conservées : une mécanique par tâche, livrée entière et fusionnée dans `dev` ; si une tâche déborde, on coupe ou on repousse en fin de file. Détail dans `CLAUDE.md` section 12.

**Déjà fait**
- [x] Sprint et endurance (valeurs du 3C)
- [x] Plantation et pousse des graines (Checkpoint B)
- [x] Chaudron : une recette qui transforme des ressources en une potion
- [x] Panneau de contrôles en lecture seule
- [x] Composant de santé + frapper les arbres et pierres + premier sort avec mana

**Chunk immédiat terminé** — voir « Prochaine étape » ci-dessus

**Semaine de cours (Bloc 1, S2)** : livrable du CDC

**File d'attente après S2, dans l'ordre**
1. [x] Un soldat ennemi simple : se déplace, attaque, meurt, laisse tomber des os au sol (nouvelle ressource `EResourceType::Bone`, paliers de rareté selon la force de l'ennemi — réutilise l'architecture `AResourceDrop`) — fusionné dans `dev` (PR #7)
2. [ ] Résurrection : nouvelle recette au chaudron (os + potion) → objet posable (comme un œuf de monstre) → posé librement depuis le personnage, n'importe où → serviteur qui apparaît et erre sans tâche dans la zone de la base jusqu'à assignation (item 3) ; revient automatiquement dans la zone s'il est posé en dehors. Dépend d'un nouveau concept technique : la « zone de base » (volume délimité, probablement calé sur `BP_Fences`).
   **Stockage provisoire (décidé le 2026-11-09)** : en attendant le vrai inventaire (item 7), l'objet de résurrection est une ressource comptée dans `UResourceCounterComponent`. Tout accès à ce stockage (« en ai-je un ? », « en consommer un ») doit être isolé à **un seul endroit** du code de pose, pour que le passage à l'inventaire ne touche que cet endroit.
   Livré en branches successives, chacune testable et fusionnable seule :
   - [x] 2.1 Recette os + potion → Necrospore (chaudron multi-recettes) — `feature/cauldron-recipes`
   - [x] 2.2a Pose de l'objet (touche L) → apparition d'un serviteur immobile — `feature/servant-placement`
   - [ ] 2.2b Zone de base : `UBoxComponent` qui fournit elle-même les points (tirés dans son espace local, donc valides même si la boîte est tournée). Les serviteurs l'interrogent à chaque décision, sans mémoriser ses dimensions, pour suivre un futur agrandissement de la base (items 13-14)
   - [ ] 2.2c Errance dans la zone : destination tirée par la zone puis projetée sur le NavMesh (`ProjectPointToNavigation`, essais bornés), ce qui écarte les points tombés dans un arbre ou une clôture. Le retour d'un serviteur posé hors zone en découle sans mode dédié. À vérifier : arbres (Foliage, `Can Ever Affect Navigation`) et clôture découpent bien le NavMesh (touche P), cas d'un serviteur posé sans chemin vers la base
3. [ ] Assignation des tâches aux serviteurs : menu dédié (touche à définir), deux colonnes — serviteurs non assignés à gauche, catégories de tâche à droite (« attaque/défense », « récolte de champ »...). La récolte automatique devient une conséquence de l'assignation à « récolte de champ », pas un comportement indépendant. Ferme la boucle de jeu principale, combiné à l'item 2
   - [ ] 3.1 Menu d'assignation (deux colonnes, sans comportement derrière au début)
   - [ ] 3.2 Comportement « récolte de champ » (le serviteur interagit avec `AFieldPlot`)
3b. [ ] Spawner de vagues d'ennemis (manquait dans la file, prérequis de l'item 16 et du rôle « attaque/défense »). Inclut `AEnemySoldier::SetPathSplineActor(AEnemyPathSpline*)` : `PathSplineActor` est `EditInstanceOnly`, donc un soldat spawné par code doit recevoir sa spline autrement. **Piège** : `AEnemyAIController::OnPossess` lit la spline au moment de la possession, et avec `AutoPossessAI` la possession a lieu *pendant* `SpawnActor` — un setter appelé après `SpawnActor` arriverait trop tard. Utiliser `SpawnActorDeferred` → setter → `FinishSpawning` (même principe que `AResourceDrop::InitializeDrop`), ou faire lire la spline plus tard par le contrôleur
4. [ ] Menu de paramètres complet : son, FOV, remappage réel des touches (Enhanced Input, Player Mappable Keys)
5. [ ] Système de sauvegarde (détail ci-dessous)
6. [ ] UI hors du Character (PlayerController/HUD) + despawn des drops au sol après délai
7. [ ] Vrai inventaire (modèle de données : stacks, slots ; UI liste simple)
8. [ ] UI drag-and-drop sur l'inventaire + barre de raccourcis (9 emplacements)
9. [ ] Objets équipables + contrôles contextuels selon l'objet en main
10. [ ] Roue de sorts (clic droit maintenu) + mana généralisé (4 emplacements, recharge ~5s)
11. [ ] Sort 2 + Sort 3
12. [ ] Sort 4
13. [ ] Vue de gestion top-down : mise en pause du jeu à l'entrée. Indissociable de l'item 14, à livrer avec lui
14. [ ] Construction de base façon city-builder (grille de cases virtuelles, placement, rotation, déplacement d'un bâtiment déjà posé, aperçu vert/rouge selon la validité de la case). Les cases du chemin unique vers le village sont verrouillées non constructibles en permanence (`GDD.md` section 3, pilier B). Indissociable de l'item 13
15. [ ] Construction ↔ ressources (coût, effets des bâtiments)
16. [ ] Zone de fin : trigger box le long du chemin existant, accessible une fois un seuil de vagues survécues atteint (valeur à déterminer par playtest), avec indicateur visuel quand la condition est remplie. Remplace l'idée initiale de village jouable + patrouilles (`GDD.md` section 2)
17. [ ] Condition de victoire + clôture narrative : en entrant dans la zone de fin (item 16), choix binaire (terminer la run / continuer à jouer). Si le joueur termine : cinématique à variantes (petite/moyenne/grande armée, choisie par le nombre de serviteurs au moment du déclenchement) montrant la conquête du village — victoire teintée, le joueur reste l'antagoniste (`GDD.md` section 2)

À chaque étape significative : tag et Release GitHub (`v0.2`, `v0.3`...).
 
## Système de sauvegarde
 
À concevoir avant les mécaniques de progression. Données à persister :

- [ ] État des objets déplacés (position et rotation des objets saisissables, comme `BP_Rock`)
- [ ] État des objets interactables (nœuds déjà récoltés, drops encore au sol)
- [ ] Narration : `bHasTriggered` de chaque `AStoryTrigger`, pour qu'un texte déjà lu ne réapparaisse pas
- [ ] Ressources du `UResourceCounterComponent`
- [ ] Plus tard : plantations en cours de pousse, serviteurs, potions, état de la base

## Dette technique
 
- [ ] Supprimer `AEnemySoldier::DebugKill` (fonction de debug temporaire, ajoutée le 2026-11-10 pour tester le chaudron) **et la touche de debug qui l'appelle dans `BP_FirstPersonCharacter`** dès que le joueur a un vrai moyen de tuer un soldat — à ne pas laisser dans un build livré
- [ ] Supprimer le nœud `Add Resource (Necrospore)` de test dans le `BeginPlay` de `BP_FirstPersonCharacter` (ajouté le 2026-11-10 pour tester la pose des serviteurs) — à ne pas laisser dans un build livré
- [ ] Déplacer la création de l'UI hors du personnage (PlayerController ou HUD) quand l'interface grossira
- [ ] Faire disparaître les drops restés au sol après un délai (avant l'item 3.2, quand les serviteurs récolteront)
- [ ] Vérifier / faire la factorisation du calcul de la cible dans `UGrabComponent` (`GetGrabTargetTransform`)
- [ ] Adopter `TObjectPtr` pour les membres `UPROPERTY` dans `Interaction/` et `Core/Character/` (déjà fait dans tout `Resources/` ; adoption par domaine, pas une bascule globale — voir `CLAUDE.md` section 6)
- [ ] Nettoyage optionnel des assets inutilisés du template (animations `Pistol`...) : branche `chore/`, suppression via le Content Browser en vérifiant les références

## Limites connues (acceptées pour l'instant)
 
- Push n'est pas une action indépendante : fusionné dans le grab
- Un objet tenu peut traverser des obstacles fins si le joueur bouge vite
- Le redressement d'un drop à l'atterrissage est instantané
- Sur une pente, un drop se pose droit et non aligné sur le terrain
- Le champignon ne se fracture qu'en 2 morceaux
- Les couleurs des morceaux du champignon ne s'affichent que dans l'éditeur

## Bloc 2 : visuel et son
 
- [ ] Matériaux réels : champignon, drops, nœuds récoltables
- [ ] Apparence de l'UI (compteur de ressources, texte narratif)
- [ ] Son et effet au ramassage d'un drop
- [ ] Rotation ou flottement des drops au sol
- [ ] Animation douce du redressement des drops à l'atterrissage
- [ ] Particules d'impact en frappant les arbres et les pierres
- [ ] Tremblement de l'objet frappé (évoqué pendant le développement du premier sort, reporté faute d'animation/VFX à ce stade)
- [ ] Effets visuels des sorts (Niagara)
- [ ] Plus de fragments sur le champignon si besoin
- [ ] Remplacer le `DrawDebugLine` de `UInteractionComponent` par un vrai retour visuel joueur (surbrillance/réticule) pour indiquer une cible saisissable/interactable
- [ ] Matériau réel pour `AFieldPlot` (remplace la sphère de debug colorée par état)
- [ ] Habiller les abords au-delà de la zone jouable (colline + abords immédiats, voir `GDD.md` section 3) pour éviter un vide visuel : forêt dense, brouillard de distance, ou relief fermant la vue

## Idées de gameplay à rediscuter
 
- [ ] Attraction des drops vers le joueur à proximité
- [ ] Fusion des drops identiques proches
- [ ] Repousse des arbres et des pierres (à réfléchir avec la plantation)
- [ ] Interface de sélection de recette pour le chaudron (pertinente à partir d'au moins deux recettes)
- [ ] Idée initiale du chaudron (UI drag-and-drop, inventaire à droite / cases de recette à gauche) mise de côté : dépend d'un vrai système d'inventaire (voir file d'attente, point 7-8)
 