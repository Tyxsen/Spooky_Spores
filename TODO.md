# TODO — Spooky_Spores
 
## Prochaine étape
 
- [x] Panneau de contrôles en lecture seule (affichage des touches actuelles, toggle Entrée, pas de remappage pour l'instant) — terminé, testé, fusionné dans `dev`
- [x] Composant de santé + frapper les arbres et pierres + premier sort avec mana — terminé et testé (non-régression comprise), sur `feature/health-and-combat`, prêt à fusionner dans `dev`
- [x] Semaine de cours S2 : environnement du joueur (colline, cabane) livré, Release `V1.0.1` — fusionné dans `dev`
- [x] GDD complet (`GDD.md`, 5 sections) — rédigé du 2026-10-26 au 2026-11-03. La file d'attente ci-dessous a été réorganisée en conséquence (items 1, 2, 3, 13, 14, 16, 17 mis à jour). Le point de passage obligatoire (`CLAUDE.md` section 12) est levé.
- [x] Item 1 (soldat ennemi simple) sur `feature/enemy-soldier` : code terminé et testé en PIE (suivi de spline, détection/approche du destructible le plus proche, attaque, mort, drop d'os via `AResourceDrop`). Pas encore fusionné dans `dev`.
- [ ] Ajouter `AEnemySoldier::SetPathSplineActor(AEnemyPathSpline*)` : `PathSplineActor` est `EditInstanceOnly` (assignable seulement sur une instance déjà placée dans le niveau), donc un ennemi spawné dynamiquement par un futur spawner de vagues n'a aucun moyen de recevoir cette référence par l'éditeur — le spawner devra l'appeler juste après `SpawnActor`, sur le principe de `AResourceDrop::InitializeDrop`

## Divers à régler
 
- [ ] `CLAUDE.md` : préciser quels projets de l'année doivent être rendus en anglais (dépend des CDC pas encore communiqués)
- [ ] Passer Unreal en 5.8.3 sur l'autre ordinateur
- [x] Supprimer la branche `feature/plantation` sur `origin` (déjà fusionnée dans `dev`)

## Planning du Bloc 1

Refait le 2026-10-07 (ancien découpage par « périodes » abandonné). Détail du raisonnement dans `CLAUDE.md` section 12. Règles : une mécanique par tâche, livrée entière et fusionnée dans `dev` ; les semaines de cours sont réservées au CDC ; si une tâche déborde, on coupe ou on repousse en fin de file ; **tout est visé pour le Bloc 1**, pas de report présumé vers le Bloc 2.

**Déjà fait**
- [x] Sprint et endurance (valeurs du 3C)
- [x] Plantation et pousse des graines (Checkpoint B)
- [x] Chaudron : une recette qui transforme des ressources en une potion
- [x] Panneau de contrôles en lecture seule
- [x] Composant de santé + frapper les arbres et pierres + premier sort avec mana

**Chunk immédiat terminé** — voir « Prochaine étape » ci-dessus

**Semaine de cours (Bloc 1, S2)** : livrable du CDC

**File d'attente après S2, dans l'ordre**
1. [ ] Un soldat ennemi simple : se déplace, attaque, meurt, laisse tomber des os au sol (nouvelle ressource `EResourceType::Bone`, paliers de rareté selon la force de l'ennemi — réutilise l'architecture `AResourceDrop`)
2. [ ] Résurrection : nouvelle recette au chaudron (os + potion) → objet posable ajouté à l'inventaire (comme un œuf de monstre) → posé librement depuis le personnage, n'importe où → serviteur qui apparaît et erre sans tâche dans la zone de la base jusqu'à assignation (item 3) ; revient automatiquement dans la zone s'il est posé en dehors. Dépend d'un nouveau concept technique : la « zone de base » (volume délimité, probablement calé sur `BP_Fences`). À livrer avec l'item 3 comme un seul bloc
3. [ ] Assignation des tâches aux serviteurs : menu dédié (touche à définir), deux colonnes — serviteurs non assignés à gauche, catégories de tâche à droite (« attaque/défense », « récolte de champ »...). La récolte automatique devient une conséquence de l'assignation à « récolte de champ », pas un comportement indépendant. Ferme la boucle de jeu principale, combiné à l'item 2
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

**Semaine de cours (Bloc 1, S3)** : livrable du CDC + oral de fin de Bloc 1 — position exacte dans la file inconnue pour l'instant (nombre de semaines d'alternance restantes non précisé).

À chaque étape significative : tag et Release GitHub (`v0.2`, `v0.3`...).
 
## Système de sauvegarde
 
À concevoir avant les mécaniques de progression. Données à persister :

- [ ] État des objets déplacés (position et rotation des objets saisissables, comme `BP_Rock`)
- [ ] État des objets interactables (nœuds déjà récoltés, drops encore au sol)
- [ ] Narration : `bHasTriggered` de chaque `AStoryTrigger`, pour qu'un texte déjà lu ne réapparaisse pas
- [ ] Ressources du `UResourceCounterComponent`
- [ ] Plus tard : plantations en cours de pousse, serviteurs, potions, état de la base

## Dette technique
 
- [ ] Déplacer la création de l'UI hors du personnage (PlayerController ou HUD) quand l'interface grossira
- [ ] Faire disparaître les drops restés au sol après un délai (avant les serviteurs)
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
 