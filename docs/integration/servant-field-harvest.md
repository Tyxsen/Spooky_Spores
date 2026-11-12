# Intégration éditeur — servant-field-harvest (2.2a)

**Moins de 5 étapes [MCP] et aucune valeur de composant héritée du C++ à poser : ne pas lancer `bp-integrator`. L'humain fait les étapes 1 à 6 lui-même (vérifications, au plus une correction).**
Aucun Blueprint à créer. Éditeur injoignable à la rédaction (`ue_project_info` : « No Unreal Editor instances found », cause connue : serveur MCP à déclarer avec `UEMCP_MULTICAST_BIND=127.0.0.1`, `docs/WORKFLOW_AGENTS.md` §6). État réel des `AFieldPlot`, de `BP_BaseZone` et du NavMesh : **non constaté**, checklist déduite du code et des docs.

## 1. Pré-requis
- Branche `feature/servant-field-harvest`, éditeur fermé pendant le switch/pull.
- Classes C++ à voir dans l'éditeur : `UFieldPlotSubsystem`, `AFieldPlot::IsReadyToHarvest`, `AServantAIController::GetHarvestTarget/IsHarvesting`, `EServantWanderAction::Harvest`, catégorie `Servant|Harvest`.
- Assets existants : `/Game/Spooky_Spores/Placeables/Resources/BP_FieldPlot`, `BP_BaseZone` (`Placeables/Base/`), `BP_Servant`, `Lvl_Principal` (`Maps/`).

## 2. Compilation et rechargement (humain)
1. Fermer l'éditeur. 2. Régénérer les fichiers de projet (clic droit `.uproject`). 3. **Rebuild complet** dans Rider (nouvelle valeur d'enum + `UFUNCTION`, pas de Live Coding). 4. Lancer les Automation Specs (filtre `SpookySpores`) : toutes vertes. 5. Rouvrir l'éditeur sur la branche, ouvrir `Lvl_Principal`.

## 3. Étapes
1. **[HUMAIN]** `BP_ServantAIController`/`BP_Servant` : ouvrir `BP_Servant`, onglet Class Defaults, `AI Controller Class` = `AServantAIController` (ou enfant). Attendu : `Servant|Harvest` visible avec `HarvestReach` 100, `HarvestProjectionExtent` 100, `HarvestNextDelay` 0.5. Aucune valeur d'instance à poser. (Je ne sais pas si le contrôleur est un Blueprint : nom non confirmé.)
2. **[HUMAIN]** `Lvl_Principal`, Outliner : lister les `BP_FieldPlot` placés. Attendu : au moins un champ.
3. **[HUMAIN]** Pour chaque champ : sélectionner et lire `Farming`. Attendu : `HarvestDrops` à jour (au moins `Wheat`, `Amount`/`SpawnCount` voulus, ex. `Wheat` x N ; une entrée par type), `SeedCost` voulu. Si une propriété est en gras (surchargée à la main), la comparer au défaut du Blueprint ; `Reset to Default` si périmée (`CLAUDE.md` §8). Le crédit lit `Type`/`Amount`/`SpawnCount`, pas `DropClass`.
4. **[HUMAIN]** Pour chaque champ : sa position est dans la boîte de `BP_BaseZone` en X et Y (la hauteur est ignorée, `bIgnoreHeight`). Attendu : `IsPointInZone` vrai ; sinon le champ n'est jamais éligible.
5. **[HUMAIN]** Touche P (Show > Navigation) dans la vue : chaque champ est couvert par le `NavMeshBoundsVolume` (zone verte) à moins de `HarvestProjectionExtent` (100 cm) d'un point navigable. Le NavMesh a été réduit à la base à la main : si un champ est hors volume ou en marge d'exclusion, il est inaccessible.
6. **[HUMAIN, seulement si 3, 4 ou 5 échoue]** Corriger : déplacer le champ ou agrandir le volume, ou monter `HarvestProjectionExtent` (garder <= `HarvestReach`), ou `Default Query Extent` (`Project Settings > Navigation System`). Modifier `Lvl_Principal.umap` seulement ici. Sauvegarder, un seul commit niveau.
7. **[MCP, conditionnel à 3 seulement]** Poser `HarvestDrops` d'un `AFieldPlot` d'instance (propriété d'acteur) : seul cas où `bp-integrator` serait utile, et seulement si le tableau de structs est supporté (non confirmé : dans le doute, humain).

## 4. Vérifications lecture seule (`editor-verifier`, éditeur ouvert)
`ue_list_actors` classe `BP_FieldPlot_C` : nombre et positions ; `ue_get_actor` `BP_BaseZone` : transform et extent ; comparer X/Y de chaque champ. Lire `HarvestDrops`/`SeedCost` d'instance si exposés. Le NavMesh se constate visuellement (étape 5), non lisible par ces outils : le dire.

## 5. Scénario PIE (Timéo)
Compteur visible : `WBP_ResourceCounter`. Debug serviteur : `LastAction`, `bIsHarvesting` (`VisibleInstanceOnly`).
1. **Récolte complète** : planter un champ (E, graine dépensée), attendre `Grown` (`GrowthDuration`), poser un serviteur (L), Tab, assigner `FieldHarvest`. Attendu : il marche vers le champ, s'arrête à ~`HarvestReach`, compteur +`Amount*SpawnCount` par type, champ `Empty` (sphère grise), **aucun drop au sol**, puis errance.
2. **Latence** : champ mûr, serviteur déjà `FieldHarvest` : récolte jusqu'à ~5 s plus tard (`MaxWanderPause` + trajet). Attendu, pas un bug.
3. **Dé-assignation par Tab en route** : repasser `Unassigned`/`AttackDefense` pendant le trajet. Attendu : arrêt net, aucun crédit, champ `Grown`. Un 2e serviteur `FieldHarvest` peut ensuite le prendre.
4. **Champ récolté par le joueur en route** : E sur le champ. Attendu : drops au sol (chemin joueur inchangé), pas de crédit serviteur, retour à l'errance sans blocage.
5. **Deux serviteurs, un champ** : un seul crédité, le second erre.
6. **Champ hors NavMesh** (ou test sur un champ déplacé temporairement, sans sauvegarder) : le serviteur erre, pas de boucle ni blocage (`bSkipNextHarvest`), champ `Grown`.
7. **Distance d'arrêt** : s'arrête à `HarvestReach` + capsule du champ, pas collé ; ajuster `Servant|Harvest` sur `BP_Servant` (ou contrôleur) : `HarvestReach`, `HarvestProjectionExtent` (<= `HarvestReach`), `HarvestNextDelay`.
8. **Mécaniques impactées (diff)** : errance normale d'un serviteur `Unassigned` (inchangée), retour en zone (`Return` avant récolte : l'emmener hors base), menu Tab (assignation, pause), récolte du joueur sur un champ, plantation (compteur de graines).

## 6. Risques
- `Lvl_Principal.umap` : point chaud, **une seule branche à la fois** ; pas de modification si étapes 2 à 5 passent.
- Critère « serviteur en route avec NavMesh » non testé automatiquement : le PIE 3, 4, 6 est la seule couverture.
- Crédit au joueur 0 (`PROVISIONAL`) : sans pawn ni compteur, le serviteur ne réserve pas.
