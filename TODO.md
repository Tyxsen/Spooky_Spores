# TODO — Spooky_Spores

Seul le reste à faire figure ici. Le déjà fait est dans `git log` et `documentation-technique.md`. Pas de découpage par semaines : on déroule la file (règles : `CLAUDE.md` section 9). Release GitHub à chaque étape significative (`v0.2`, `v0.3`...).

## En cours
(rien)

## File d'attente (dans l'ordre)
3b. **Ennemis : déplacement NavMesh en `Engage` + réservation souple des cibles** (demandes de Timéo après son test PIE du spawner). Timéo : « ils font un move to location sur les objets destructibles, ils ne prennent pas en compte le terrain et là par exemple j'ai mis l'objet derrière un arbre, il se sont coincés dessus au lieu de le contourner. Et pour la détection d'objets, j'aimerais que ça détecte des objets qui ne sont pas déjà en train d'être détruits par d'autres soldats. Sinon ils vont à 4 sur un même objet et se coince entre eux etc. »
   - Fait établi : en `Engage`, le soldat avance par `AddMovementInput` en ligne droite (pas de NavMesh). Le passage au pathfinding réutilise les enseignements de `CLAUDE.md` section 8 : `bUseAccelerationForPaths`, arrivée mesurée en 2D, `bStopOnOverlap` (rayon de capsule déjà ajouté), `Default Query Extent`, `OnMoveCompleted` synchrone.
   - Réservation **souple** (décision de Timéo) : chaque soldat prend la cible libre la plus proche ; s'il n'y en a aucune, il rejoint la moins occupée. La réservation se pose au choix de la cible ; elle se libère à la mort du soldat, à la mort de la cible et au changement de cible. Distinguer « aucun destructible vivant » (erreur, log existant) de « tous réservés » (normal, pas d'erreur).
   - Limites de test : les mondes de test n'ont pas de NavMesh (comme pour les serviteurs) : les tests de ciblage devront lire la cible choisie, pas la position du soldat.
   - À vérifier : le NavMesh couvre-t-il la fin de la spline et la base ?
2. **Assignation des tâches aux serviteurs** : menu à deux colonnes (serviteurs non assignés / catégories « attaque-défense », « récolte de champ », potentiellement d'autres si besoin ou bonnes idées). La récolte automatique découle de l'assignation, pas d'un comportement indépendant. Ferme la boucle principale (résurrection, pose, errance : faits).
   - [x] 2.1 Menu · [x] 2.2a Récolte de champ (le serviteur assigné `FieldHarvest` récolte les champs `Grown` et crédite directement le compteur du joueur) · [x] 2.2b Replantage (le serviteur récolte et replante sur place ; crédit puis dépense, le reste des graines reste au compteur du joueur)
   - [ ] 2.2c Rôle « attaque/défense » (le spawner d'ennemis existe). Applique la règle d'errance de Timéo (ci-dessous) : le serviteur quitte l'errance seulement s'il y a des ennemis vivants à tuer, et y revient quand il n'y en a plus. Demande la détection des ennemis vivants. À faire après 3b : dépendance de confort, pas prérequis dur (le serviteur n'a pas besoin du NavMesh ennemi pour fonctionner, mais 3b stabilise le ciblage partagé et le déplacement des ennemis).
   - Règle d'errance (Timéo, pour 2.2 et attaque/défense ; récolte : faite en 2.2a ; attaque/défense : 2.2c) : « Alors en soit l'assignation des tâches ne doit pas rééllement enlever l'état d'errance, il l'enlève seulement si il y a des fieldPlot à gérer dans la base ou si il y a des ennemis à tuer. Dans le cas où il y a plus de champ ou plus d'ennemis, le serviteur se remet dans un êtat d'errance même en étant assigné, dans l'attente de nouvelles choses à faire. »
4. Menu de paramètres : son, FOV, remappage des touches (Enhanced Input, Player Mappable Keys)
5. Sauvegarde (voir plus bas)
6. UI hors du Character (PlayerController/HUD) + despawn des drops au sol après délai
7. Vrai inventaire (stacks, slots, UI liste)
   - Cible de Timéo : « un objet dans la base à côté du coeur du joueur qui va en fait être un inventaire virtuel à distance qui sera en fait mon inventaire et celui des serviteurs, une sorte d'inventaire partagé en remote dans lequel les serviteurs pourront piocher pour replanter ». Décision de récolte : « quand c'est le serviteur qui récolte, les drops n'apparaissent pas mais au contraire que ça s'ajoute directement dans l'inventaire partagé ».
   - Stockage provisoire (décidé le 2026-11-09) : c'est le compteur du joueur 0 (`UResourceCounterComponent`), la Necrospore y est une ressource comptée. Chaque accès reste isolé à un endroit précis, pour que le passage au vrai inventaire ne touche que ceux-là : pose de Necrospore, `DoPlaceServant` (`TrySpendResource`) ; récolte serviteur, dans `AServantAIController` (même fichier), `FindHarvestStorage` (résolution), `HarvestAndDeposit` (crédit), `TryPlantOnField` (dépense des graines) et `WatchStorage` (abonnement à `OnResourceChanged`).
8. UI drag-and-drop + barre de raccourcis (9 emplacements). Le menu des serviteurs passe de la sélection par clic au glisser-déposer (seul le widget change, l'API C++ reste)
9. Objets équipables + contrôles contextuels
10. Roue de sorts (clic droit maintenu) + mana généralisé (4 emplacements, recharge ~5 s)
11. Sorts 2 et 3
12. Sort 4
13. Vue de gestion top-down (pause à l'entrée) — **indissociable de l'item 14**. Reprendre la pause du menu des serviteurs (fonction privée du Character), premier consommateur
14. Construction façon city-builder (grille, placement, rotation, déplacement, aperçu vert/rouge). Cases du chemin vers le village verrouillées en permanence (`GDD.md` section 3). Indissociable du 13. L'agrandissement de la base doit mettre à jour `BP_BaseZone`.
15. Construction ↔ ressources (coût, effets des bâtiments)
16. Zone de fin : trigger box le long du chemin, accessible après un seuil de vagues survécues (à fixer par playtest) **et** accès garanti quand `AEnemyWaveSpawner::OnAllWavesCleared` est émis (20 vagues tuées par défaut, village vide : le joueur a gagné par défaut, même sans la puissance nécessaire, `GDD.md` section 4), indicateur visuel (`GDD.md` section 2)
17. Victoire + clôture narrative : à l'entrée de la zone de fin, choix terminer / continuer ; cinématique à variantes selon le nombre de serviteurs (`GDD.md` section 2)

## Sauvegarde (à concevoir avant la progression)
Positions des objets saisissables · nœuds récoltés et drops au sol · `bHasTriggered` des `AStoryTrigger` · ressources du `UResourceCounterComponent` · plus tard : plantations, serviteurs, potions, état de la base. Données externes : c'est ici qu'on durcit (NaN, valeurs hors bornes).

## Dette technique
- [ ] **Avant toute Release** : supprimer `AEnemySoldier::DebugKill` et sa touche dans `BP_FirstPersonCharacter`, ainsi que la touche de test `StartWaves` (même Blueprint) ; supprimer le nœud de test `Add Resource (Necrospore)` du `BeginPlay` de `BP_FirstPersonCharacter` ; remettre `Hidden In Game` à `true` sur `ZoneBox` de `BP_BaseZone`
- [ ] Placeholders du spawner de vagues à régler (instance dans `Lvl_Principal`) : `Count` des 20 entrées de `Waves` (3 chacune aujourd'hui) ; `FirstWaveDelay` 180 s non validé ; `WaveInterval` 180 s à réécrire au game feeling
- [ ] Déplacer la création de l'UI hors du personnage (item 6), y compris le menu des serviteurs et sa pause
- [ ] `UServantTaskTestListener` (`Tests/`) est compilée dans toutes les configurations (UHT refuse une `UCLASS` sous `#if`) : à revoir si un module de test séparé voit le jour
- [ ] Faire disparaître les drops restés au sol après un délai (drops du joueur : Timéo « Oui quand c'est le joueur je veux des drops quand même. », item 6)
- [ ] Divergence assumée : `AResourceDrop::SpawnDrops` (chemin joueur, drops au sol) saute les entrées de `HarvestDrops` sans `DropClass`, `AFieldPlot::ComputeHarvestYield` (chemin serviteur) les compte : un champ sans `DropClass` rapporte donc au serviteur mais rien au joueur. Timéo : « Garde l'état actuel ça me va. De toute façon ça changera bien quand je vais faire l'inventaire. » Disparaît avec l'item 7
- [ ] Non automatisé (`FTestWorldWrapper` n'a pas de NavMesh) : la récolte et la plantation hors portée, l'annulation en route (champ récolté, planté ou détruit, dé-assignation, `UnPossess` pendant le trajet) et le flag `bWakePending` levé pendant un travail de champ n'ont pas de test automatique. Testés à la main en PIE par Timéo (récolte : 2026-11-13 ; plantation, annulations et flag : date non renseignée) ; à rejouer à la main si le contrôleur change
- [ ] Non automatisé (pas de NavMesh dans les mondes de test) : le déplacement réel des ennemis (transit sur la spline, marche vers la cible, attaque) n'est testé qu'à la main en PIE (test de Timéo sur le spawner : 2026-11-19). À rejouer à la main si le spawner, le ciblage ou `BP_EnemySoldier` changent
- [ ] Factoriser le calcul de la cible dans `UGrabComponent` (`GetGrabTargetTransform`)
- [ ] `TObjectPtr` dans `Interaction/` et `Core/Character/` (par domaine, voir `CLAUDE.md` section 3)
- [ ] Nettoyer les assets inutilisés du template (animations `Pistol`...) : branche `chore/`, via le Content Browser
- [ ] `CLAUDE.md` : préciser quels projets doivent être rendus en anglais (CDC pas encore communiqués)

## Limites connues (acceptées)
Le serviteur peut vider les graines du joueur (assumé par Timéo, disparaît avec l'inventaire partagé, item 7) · annulation d'un travail de champ seulement possible avec un déplacement actif (comportement moteur `StopMovement`/`AbortMove`) · la pause après un `Return` réussi avec `bWakePending` levé n'est pas testée en Spec · Push fusionné dans le grab · un objet tenu peut traverser un obstacle fin si le joueur bouge vite · redressement d'un drop instantané · drop non aligné sur une pente · champignon fracturé en 2 morceaux seulement · couleurs des morceaux visibles uniquement dans l'éditeur · un soldat sans destructible vivant dans la base reste immobile et la vague ne se termine jamais (un seul `UE_LOG(Error)`, pas de filet de chrono : proposition rejetée par Timéo) · un soldat coincé derrière un obstacle bloque la fin de vague (levé par l'item 3b) · `DistanceAlongSpline` avance au temps et non à la position réelle du soldat (pré-existant) · cibles réelles des soldats aujourd'hui : les 2 `BP_Harvestable_Stone_Destructible` de la base (les `GC_Mushroom` ne sont pas des cibles ; le cœur du joueur n'existe pas encore)

## Plus tard (visuel, son, idées)
- **Visuel/son** : matériaux réels (champignon, drops, nœuds, `AFieldPlot`) · apparence de l'UI · son et effet au ramassage · rotation/flottement des drops · redressement doux · particules d'impact et tremblement de l'objet frappé · Niagara des sorts · plus de fragments · remplacer le `DrawDebugLine` de `UInteractionComponent` par une surbrillance/réticule · habiller les abords hors zone jouable (`GDD.md` section 3)
- **Idées à rediscuter** : attraction des drops vers le joueur · fusion des drops identiques · repousse des arbres et pierres · sélection de recette au chaudron (dès 2 recettes ; l'UI drag-and-drop attend l'inventaire, items 7-8) · écran de gestion commun à onglets (inventaire, serviteurs) ouvert par Tab, à trancher à l'item 7 (Timéo : « La solution que tu proposes d'avoir des onglets peut être très intéressante. J'y réfléchirai quand je serai à l'inventaire »)
