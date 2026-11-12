# Spec — servant-field-harvest (sous-tâche 2.2a)
Item TODO : 2.2 Comportement « récolte de champ » | Branche : feature/servant-field-harvest | Statut : brouillon

## Objectif
Un serviteur assigné à `EServantTask::FieldHarvest` quitte l'errance quand un `AFieldPlot` de la base est `Grown`, le récolte et **crédite directement le compteur du joueur** (pas de drops), puis retourne à l'errance quand plus aucun champ n'est à traiter (règle d'errance de Timéo, `TODO.md` item 2). Ferme « ... → le serviteur récolte » (`CLAUDE.md` §9). Le replantage est la sous-tâche 2.2b ; l'inventaire partagé de la base (objet narratif, Timéo D1) est hors feature.

## Comportement
- **Règle d'errance** : l'errance reste l'état de fond. À chaque décision (`MakeDecision`), si le serviteur est **dans une zone valide**, que sa tâche est `FieldHarvest`, qu'un stockage existe et qu'un champ éligible est réservable, l'action est `Harvest` ; sinon la décision existante (`Wander` / `Return` / `Teleport` / `Wait`) s'applique. `DecideNextAction` (pure) ne change pas ; hors zone, `Return` passe avant la récolte.
- **Éligible** : `Grown`, non réservé, `ABaseZone::IsPointInZone(position du champ)`. Choix : le plus proche (distance 2D). Fin de récolte = plus aucun éligible : la décision suivante tombe sur `Wander` (pas d'état « retour »).
- **Latence acceptée** : un champ mûr est vu à la décision suivante (<= `MaxWanderPause` + durée du déplacement), pas de notification du champ (choix technique : pas de Tick, KISS).
- **Réservation** : le contrôleur réserve le champ choisi (`UFieldPlotSubsystem`) avant de bouger ; il la libère dans tous les cas de fin (récolte, échec, annulation, `StopWandering`).
- **Déplacement** : si le serviteur est déjà à <= `HarvestReach` + rayon de capsule (distance 2D, `CLAUDE.md` §8), récolte immédiate sans déplacement. Sinon point du NavMesh projeté près du champ (extension `HarvestProjectionExtent`, projection faite par nous comme `TryFindNavPointInZone`, factoriser), `MoveToLocation(..., AcceptanceRadius = HarvestReach, bStopOnOverlap = true, bProjectDestinationToNavigation = false)`. Pas de projection possible : échec (voir ci-dessous).
- **Arrivée** (seul `OnMoveCompleted` planifie la suite) : `Success` et champ encore valide, prêt et réservé par ce contrôleur : récolte puis prochaine décision après `HarvestNextDelay`. Sinon (échec, `Aborted`, champ détruit, déjà récolté) : réservation libérée, `bSkipNextHarvest = true` (la décision suivante, après `RetryDelay`, ne récolte pas : évite de boucler sur un champ inaccessible), puis errance.
- **Récolte et dépôt** : une seule fonction privée du contrôleur, `HarvestAndDeposit(AFieldPlot&)`, commentée `PROVISIONAL` : résout le stockage (**joueur 0 du monde**, `UGameplayStatics::GetPlayerPawn(this, 0)` puis `FindComponentByClass<UResourceCounterComponent>`), refuse sans stockage (le champ reste `Grown`), appelle `TryHarvestYield`, puis `AddResource` par type. C'est **le seul endroit** à changer pour l'inventaire partagé (item 7). Même précédent que `DoPlaceServant`. La décision vérifie aussi le stockage avant de réserver.
- **Dé-assignation** : le contrôleur s'abonne à `UServantSubsystem::OnAssignmentsChanged` (OnPossess / OnUnPossess). Le gestionnaire (idempotent, l'événement part aussi à chaque (dé)enregistrement) annule seulement si une récolte est en cours et que la tâche n'est plus `FieldHarvest` : libère, `LastAction = None`, `StopMovement`. Un serviteur désenregistré (EndPlay) compte comme `Unassigned`.
- **`AFieldPlot` neutre** : ne connaît ni `AServant` ni le compteur. `TryHarvestYield` rend les gains sans spawner ; le chemin joueur (`OnInteract` -> drops au sol) est inchangé.
- Choix techniques : `UFieldPlotSubsystem` = `UWorldSubsystem` (`Resources/`, modèle `UBaseZoneSubsystem`, jamais `TActorIterator`) ; réservation par `TWeakObjectPtr` (champ et réservataire ; réservataire mort = champ libre) ; réservataire = le contrôleur. Vérifié dans `ServantAIController.cpp` : le cycle `bIsWandering` + `OnMoveCompleted` supporte l'ajout sans Tick.

## Critères d'acceptation
- [ ] `IsReadyToHarvest()` est vrai pour `Grown` seulement (faux pour `Empty` et `Growing`).
- [ ] `TryHarvestYield` sur `Grown` renvoie vrai, remplit `OutYield` avec, par type, la somme `Amount * SpawnCount` de `HarvestDrops`, passe le champ à `Empty` et ne spawne aucun `AResourceDrop` ; sur `Empty`/`Growing` renvoie faux, laisse l'état et `OutYield` vide.
- [ ] Chemin joueur inchangé : `OnInteract` sur un champ `Grown` spawne les drops (un `AResourceDrop` par `SpawnCount`), passe à `Empty` et ne crédite aucun compteur.
- [ ] Sous-système : un champ placé s'enregistre, un champ détruit disparaît ; `GetUnreservedHarvestableFields` ne liste que les champs `Grown` non réservés.
- [ ] Réservation : `TryReserve` par A réussit, par B échoue tant que A tient, rejoué par A réussit ; `Release` par B n'a aucun effet, par A libère ; réservataire détruit ou champ détruit = plus de réservation.
- [ ] Serviteur `FieldHarvest` dans la zone, joueur 0 avec compteur, champ `Grown` à portée : le compteur augmente exactement de la `OutYield`, le champ est `Empty`, aucun `AResourceDrop`, `LastAction == Harvest` ; après `HarvestNextDelay` + une décision, `LastAction == Wander`.
- [ ] Même scénario avec `Unassigned` puis `AttackDefense` : champ toujours `Grown`, compteur inchangé, aucune réservation, après plusieurs décisions.
- [ ] Rien à faire : sans champ, avec champs `Empty`/`Growing`, avec un champ `Grown` hors zone, ou **sans joueur / sans compteur** : aucune réservation, champ `Grown` intact, `LastAction == Wander` (ou `Wait` sans zone).
- [ ] Deux serviteurs `FieldHarvest`, un seul champ `Grown` : le compteur n'est crédité qu'une fois, l'autre serviteur n'a jamais `Harvest` pour ce champ.
- [ ] Avec NavMesh, serviteur en route (réservation tenue) : (a) champ récolté par le joueur ou détruit, (b) dé-assignation, (c) `UnPossess` : aucun crédit, réservation libérée (un autre serviteur peut réserver), `GetMoveStatus() == Idle`, retour à l'errance sans crash.

## API exposée
`AFieldPlot` : `UFUNCTION(BlueprintPure, Category = "Farming") bool IsReadyToHarvest() const;` · `bool TryHarvestYield(TMap<EResourceType, int32>& OutYield);` (C++ seulement) · `BeginPlay`/`EndPlay` surchargés (enregistrement).
`UFieldPlotSubsystem` (`UWorldSubsystem`) : `RegisterField(AFieldPlot*)`, `UnregisterField(AFieldPlot*)` (appelés par `AFieldPlot` seul) · `UFUNCTION(BlueprintPure) TArray<AFieldPlot*> GetUnreservedHarvestableFields() const;` · `bool TryReserve(AFieldPlot* Field, const AActor* Reserver);` · `void Release(AFieldPlot* Field, const AActor* Reserver);` · `UFUNCTION(BlueprintPure) bool IsReserved(const AFieldPlot* Field) const;`
`AServantAIController` : `EServantWanderAction::Harvest` (ajouté **en dernier**) · `UFUNCTION(BlueprintPure) AFieldPlot* GetHarvestTarget() const;` · `UFUNCTION(BlueprintPure) bool IsHarvesting() const;` · gestionnaire `UFUNCTION()` privé d'`OnAssignmentsChanged`.

## Valeurs réglables
Sur `AServantAIController`, catégorie `Servant|Harvest`, `EditAnywhere` + `AllowPrivateAccess` : `HarvestReach` : 100 · `HarvestProjectionExtent` : 100 (<= `HarvestReach`, sinon le serviteur s'arrête trop loin) · `HarvestNextDelay` : 0.5. La vitesse reste `WanderWalkSpeed`.

## Cas limites
- Serviteur dans la zone mais NavMesh absent près du champ : échec de projection = `bSkipNextHarvest`, il erre ; pas de blocage.
- Le NavMesh de `Lvl_Principal` est réduit à la base : un champ hors volume est inaccessible (même traitement).
- Aucun test existant ne casse : tâche par défaut `Unassigned` ; `ServantWanderWorldSpec` n'a pas de joueur ni de champ.
- Test world : `FTestWorldWrapper` n'a pas de NavMesh (`ServantWanderWorldSpec`). `qa-test-writer` doit en construire un (volume + sol) pour le dernier critère ; si impossible, il le signale (couvert alors par PIE) et teste la partie sous-système par appels directs. Je n'ai pas vérifié la faisabilité. Joueur 0 : un `APlayerController` + pawn avec compteur ; idem non vérifié.
- Champs sans `DropClass` : crédités quand même (le crédit lit `Type`/`Amount`/`SpawnCount`, pas la classe).

## Périmètre de fichiers
À créer : `Resources/FieldPlotSubsystem.{h,cpp}` · `Tests/FieldPlotSpec.cpp` (critères 1-5) · `Tests/ServantHarvestWorldSpec.cpp` (6-10, réutilise `ServantWanderTestHelpers.h`).
À modifier : `Resources/FieldPlot.{h,cpp}` · `Servants/ServantAIController.{h,cpp}`.
Points chauds : aucun côté code (`EResourceType`, `*.Build.cs`, `Default*.ini` intacts). Fin de branche, par `consistency-keeper` : `documentation-technique.md` (§6, §12 : l'assignation touche désormais le contrôleur, via abonnement) et `TODO.md` (2.2 -> 2.2a fait, ajouter 2.2b). Suppression de la spec par l'orchestrateur.
Éditeur (après compilation, **Rebuild complet** : nouvelle valeur d'enum + `UFUNCTION`) : aucun Blueprint à créer ; vérifier que les `AFieldPlot` placés dans `Lvl_Principal` sont dans `ABaseZone` et sur le NavMesh, et que leurs `HarvestDrops` d'instance sont à jour (`CLAUDE.md` §8). `Lvl_Principal.umap` (point chaud) seulement si un champ ou le volume NavMesh doit bouger. PIE (Timéo) : planter, attendre `Grown`, assigner par Tab, voir le compteur monter sans drop, puis l'errance.

## Hors périmètre / pour 2.2b (replantage)
- Replanter, objet narratif d'inventaire partagé de la base, vrai inventaire (item 7), despawn des drops, attaque/défense, animation de récolte.
- Questions ouvertes 2.2b : stock de graines à zéro (le serviteur attend-il, laisse-t-il le champ `Empty`, erre-t-il ?) · graines utilisées ailleurs (chaudron, plantation du joueur) : priorité au joueur, réserve minimale de graines ? · le serviteur récolte-t-il les graines qu'il vient de produire pour replanter (boucle autosuffisante) ? · replanter le même champ tout de suite ou récolter tous les champs d'abord ? · réservation du champ `Empty` pendant la plantation (même registre) · source des graines quand l'inventaire partagé existera.
