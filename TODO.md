# TODO — Spooky_Spores

Seul le reste à faire figure ici. Le déjà fait est dans `git log` et `documentation-technique.md`. Pas de découpage par semaines : on déroule la file (règles : `CLAUDE.md` section 9). Release GitHub à chaque étape significative (`v0.2`, `v0.3`...).

## En cours
(rien)

## File d'attente (dans l'ordre)
1. **Résurrection** (2.1 chaudron os+potion → Necrospore, 2.2a pose touche L, 2.2b zone de base, 2.2c errance : faits)
   - Stockage provisoire (décidé le 2026-11-09) : la Necrospore est une ressource comptée dans `UResourceCounterComponent`. Tout accès à ce stockage (« en ai-je un ? », « en consommer un ») reste isolé **à un seul endroit** du code de pose, pour que le passage au vrai inventaire (item 6) ne touche que cet endroit.
2. **Assignation des tâches aux serviteurs** : menu à deux colonnes (serviteurs non assignés / catégories « attaque-défense », « récolte de champ », potentiellement d'autres si besoin ou bonnes idées). La récolte automatique découle de l'assignation, pas d'un comportement indépendant. Ferme la boucle principale avec l'item 1.
   - [x] 2.1 Menu (sans comportement derrière) · [ ] 2.2 Comportement « récolte de champ » (interaction avec `AFieldPlot`)
   - Règle d'errance (Timéo, pour 2.2 et attaque/défense) : « Alors en soit l'assignation des tâches ne doit pas rééllement enlever l'état d'errance, il l'enlève seulement si il y a des fieldPlot à gérer dans la base ou si il y a des ennemis à tuer. Dans le cas où il y a plus de champ ou plus d'ennemis, le serviteur se remet dans un êtat d'errance même en étant assigné, dans l'attente de nouvelles choses à faire. »
3. **Spawner de vagues d'ennemis** (prérequis de l'item 15 et du rôle « attaque/défense »). Inclut `AEnemySoldier::SetPathSplineActor` (`PathSplineActor` est `EditInstanceOnly`). **Piège** : `AEnemyAIController::OnPossess` lit la spline pendant `SpawnActor` (`AutoPossessAI`) : utiliser `SpawnActorDeferred` → setter → `FinishSpawning`, ou faire lire la spline plus tard par le contrôleur. Réévaluer `MoveToLocation` pour l'ennemi (cause du glissement identifiée en 2.2c, voir `CLAUDE.md` section 8).
4. Menu de paramètres : son, FOV, remappage des touches (Enhanced Input, Player Mappable Keys)
5. Sauvegarde (voir plus bas)
6. UI hors du Character (PlayerController/HUD) + despawn des drops au sol après délai
7. Vrai inventaire (stacks, slots, UI liste)
8. UI drag-and-drop + barre de raccourcis (9 emplacements). Le menu des serviteurs passe de la sélection par clic au glisser-déposer (seul le widget change, l'API C++ reste)
9. Objets équipables + contrôles contextuels
10. Roue de sorts (clic droit maintenu) + mana généralisé (4 emplacements, recharge ~5 s)
11. Sorts 2 et 3
12. Sort 4
13. Vue de gestion top-down (pause à l'entrée) — **indissociable de l'item 14**. Reprendre la pause du menu des serviteurs (fonction privée du Character), premier consommateur
14. Construction façon city-builder (grille, placement, rotation, déplacement, aperçu vert/rouge). Cases du chemin vers le village verrouillées en permanence (`GDD.md` section 3). Indissociable du 13. L'agrandissement de la base doit mettre à jour `BP_BaseZone`.
15. Construction ↔ ressources (coût, effets des bâtiments)
16. Zone de fin : trigger box le long du chemin, accessible après un seuil de vagues survécues (à fixer par playtest), indicateur visuel (`GDD.md` section 2)
17. Victoire + clôture narrative : à l'entrée de la zone de fin, choix terminer / continuer ; cinématique à variantes selon le nombre de serviteurs (`GDD.md` section 2)

## Sauvegarde (à concevoir avant la progression)
Positions des objets saisissables · nœuds récoltés et drops au sol · `bHasTriggered` des `AStoryTrigger` · ressources du `UResourceCounterComponent` · plus tard : plantations, serviteurs, potions, état de la base. Données externes : c'est ici qu'on durcit (NaN, valeurs hors bornes).

## Dette technique
- [ ] **Avant toute Release** : supprimer `AEnemySoldier::DebugKill` et sa touche dans `BP_FirstPersonCharacter` ; supprimer le nœud de test `Add Resource (Necrospore)` du `BeginPlay` de `BP_FirstPersonCharacter` ; remettre `Hidden In Game` à `true` sur `ZoneBox` de `BP_BaseZone`
- [ ] Déplacer la création de l'UI hors du personnage (item 6), y compris le menu des serviteurs et sa pause
- [ ] `UServantTaskTestListener` (`Tests/`) est compilée dans toutes les configurations (UHT refuse une `UCLASS` sous `#if`) : à revoir si un module de test séparé voit le jour
- [ ] Faire disparaître les drops restés au sol après un délai (avant l'assignation « récolte de champ »)
- [ ] Factoriser le calcul de la cible dans `UGrabComponent` (`GetGrabTargetTransform`)
- [ ] `TObjectPtr` dans `Interaction/` et `Core/Character/` (par domaine, voir `CLAUDE.md` section 3)
- [ ] Nettoyer les assets inutilisés du template (animations `Pistol`...) : branche `chore/`, via le Content Browser
- [ ] `CLAUDE.md` : préciser quels projets doivent être rendus en anglais (CDC pas encore communiqués)

## Limites connues (acceptées)
Push fusionné dans le grab · un objet tenu peut traverser un obstacle fin si le joueur bouge vite · redressement d'un drop instantané · drop non aligné sur une pente · champignon fracturé en 2 morceaux seulement · couleurs des morceaux visibles uniquement dans l'éditeur

## Plus tard (visuel, son, idées)
- **Visuel/son** : matériaux réels (champignon, drops, nœuds, `AFieldPlot`) · apparence de l'UI · son et effet au ramassage · rotation/flottement des drops · redressement doux · particules d'impact et tremblement de l'objet frappé · Niagara des sorts · plus de fragments · remplacer le `DrawDebugLine` de `UInteractionComponent` par une surbrillance/réticule · habiller les abords hors zone jouable (`GDD.md` section 3)
- **Idées à rediscuter** : attraction des drops vers le joueur · fusion des drops identiques · repousse des arbres et pierres · sélection de recette au chaudron (dès 2 recettes ; l'UI drag-and-drop attend l'inventaire, items 7-8) · écran de gestion commun à onglets (inventaire, serviteurs) ouvert par Tab, à trancher à l'item 7 (Timéo : « La solution que tu proposes d'avoir des onglets peut être très intéressante. J'y réfléchirai quand je serai à l'inventaire »)
