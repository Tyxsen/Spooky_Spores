# Spec — Servant combat (rôle attaque/défense)
Item TODO : 2.2c Rôle « attaque/défense » | Branche : feature/servant-combat | Statut : validée par Timéo

## Objectif
Le serviteur assigné `AttackDefense` quitte l'errance pour tuer les ennemis vivants de la base, y revient quand il n'y en a plus (règle d'errance de Timéo, GDD « Serviteurs : peuvent être tués »). Les serviteurs deviennent des cibles des ennemis. Dépend de 3b (ciblage, `MoveToLocation`) et de 2.2a (errance).

## Comportement
- **Cible des ennemis** : `AServant` implémente `IDestructible` + un `UHealthComponent`. Vérifié dans `EnemyAIController.cpp` : `ChooseTarget` ne filtre que `Implements<UDestructible>`, vivant, origine dans `ABaseZone`. Aucun tag, aucun changement de ciblage : la règle reste celle de 3b (moins de réservations, puis le plus proche).
- **Mort** : `OnDeath` -> `Destroy()`, sans drop ni animation. `EndPlay` désenregistre déjà du `UServantSubsystem` (assignation perdue, numéro jamais réutilisé). `OnUnPossess` -> `StopWandering` libère la réservation de champ ; il doit aussi couper le combat.
- **Riposte** : seul un serviteur `AttackDefense` combat. Un serviteur en récolte attaqué continue et peut mourir (rien à coder, à tester).
- **Détection (technique)** : nouveau `UEnemyRegistrySubsystem` (modèle `UServantSubsystem`, `TWeakObjectPtr`, jamais `TActorIterator`). `AEnemySoldier` s'y enregistre en `BeginPlay`/`EndPlay`. « Vivant » = enregistré, valide, `!IsDead()`.
- **Décision** : `EServantWanderAction::Attack` (ajoutée en fin). Comme la récolte, elle ne remplace qu'un `Wander` (jamais `Return`/`Teleport`/`Wait`). Éligible : ennemi vivant dont l'origine est dans la zone (voir question 1) ; le plus proche **du serviteur** en 2D (Timéo : « pour les serviteurs ils vont sur l'ennemi le plus proche d'eux ») ; pas de réservation (plusieurs serviteurs peuvent viser le même).
- **Combat** : `bIsFighting`, `FightTarget` (faible), timer répétitif `CombatTimer` (période `AttackInterval`), premier pas immédiat. Chaque pas : cible morte, détruite ou hors zone -> fin du combat ; à portée -> `StopMovement` si en marche puis `Attack` ; hors portée sans marche en cours -> nouvelle demande. Portée = même mesure que le soldat (point de collision le plus proche moins rayon de capsule, 2D).
- **Déplacement** : `MoveToActor` (la cible bouge), derrière la couture `RequestMoveToEnemy` (virtuelle protégée, tests sans NavMesh). État écrit avant l'appel (callbacks synchrones). Vitesse `CombatWalkSpeed` pendant le combat, `WanderWalkSpeed` rétablie à la fin.
- **Fin** : `EndCombat(bFailed)` vide l'état, coupe le timer, `StopMovement` puis `ScheduleDecision` en dernier (`CombatNextDelay`, ou `RetryDelay` + `bSkipNextHarvest` en cas d'échec de déplacement). Sans ennemi, la décision suivante rend `Wander`.
- **Dé-assignation** : `OnServantAssignmentsChanged` termine aussi un combat si la tâche n'est plus `AttackDefense`.
- **Ennemi inatteignable** (revue : sans cela le scan relance en boucle un combat qui échoue) : quand `EndCombat(bFailed=true)`, l'ennemi visé est mémorisé (faible) avec l'heure ; `FindNearestEnemyInZone` l'ignore (scan et choix normal) pendant `FailedEnemyCooldown` (5 s, `Servant|Combat`), ou jusqu'à sa mort. Passé le délai, il redevient éligible.
- **Orientation** (Timéo, retour PIE : « Le serviteur ne se tourne pas vers l'ennemi si il est déjà dans la range au moment de choisir ce qu'il fait. ») : à chaque coup, `AServant` aligne son lacet (yaw seul, instantané) sur sa cible avant `Attack`, y compris quand il n'a pas marché.
- **Réveil par détection** (Timéo, retour PIE : « Si le serviteur a choisi un point et qu'il s'y rend avant qu'un ennemi rentre dans la base, il continue d'y aller jusqu'à ce qu'il l'atteigne et ensuite attaque, je voudrais que le fait de détecter un ennemi ça annule le déplacement et envoie direct sur l'ennemi. ») : tant que la tâche est `AttackDefense`, hors combat et en errance (marche ou pause, jamais `Return`/`Teleport`), un timer répétitif `EnemyScanTimer` (période `EnemyScanInterval`) consulte `UEnemyRegistrySubsystem` ; dès qu'un ennemi éligible (vivant, origine en zone) existe, la marche d'errance est annulée et le combat démarre sur le plus proche du serviteur. État écrit avant `StopMovement` (callback synchrone) ; timer coupé en combat, en `StopWandering` et à la dé-assignation.
- **Ennemi vs cible mobile** (Timéo : « J'aimerais si possible que ça suive le serviteur si il est en mouvement, pas forcément que ça check à chaque tick, peut-être toutes les secondes mais quelque chose comme ça. ») : dans `AEnemyAIController::TickEngage`, si la cible est un `APawn` hors portée, une nouvelle demande `MoveToLocation` vers sa position actuelle est émise au plus une fois par `RepathInterval` (EditAnywhere, `Enemy|Combat`, 1.0 s), sans attendre la fin du déplacement en cours. Cible non-pawn (destructible fixe) : comportement 3b inchangé. En plus, dans `OnMoveCompleted`, une fin `Success` hors portée sur un `APawn` n'exclut plus la cible 3 s (elle a bougé) ; les autres cas restent « inatteignable ».

## Critères d'acceptation
- [ ] `AServant` : `Implements<UDestructible>` vrai, `GetHealthComponent` non nul, non mort au départ.
- [ ] Un soldat en `Engage` dont le seul destructible de la zone est un serviteur le prend pour `CurrentTarget` ; après mort du serviteur, il n'a plus cette cible.
- [ ] Dégâts létaux sur un serviteur : détruit, `IsServantRegistered` faux, aucun `AResourceDrop` créé, réservation de champ en cours libérée (`IsReserved` faux).
- [ ] `UEnemyRegistrySubsystem::GetLivingEnemies` contient un soldat enregistré vivant et exclut un soldat mort ou détruit.
- [ ] Serviteur `AttackDefense` en zone + ennemi vivant en zone : à la décision suivante `IsFighting` vrai, `GetFightTarget` = l'ennemi le plus proche en 2D, `LastAction == Attack`, demande `RequestMoveToEnemy` enregistrée s'il est hors portée.
- [ ] À portée, un coup de `AttackDamage` est appliqué dès le premier pas, puis un par `AttackInterval` (pas de coup hors portée).
- [ ] Serviteur `AttackDefense` en marche d'errance, ennemi qui entre ensuite en zone : en `<= EnemyScanInterval` + marge, `IsFighting` vrai, marche d'errance annulée (pas d'arrivée au point d'errance), cible = ennemi le plus proche du serviteur. Un serviteur `Unassigned`/`FieldHarvest` ou en `Return` n'est jamais réveillé ainsi.
- [ ] Poursuite en échec (déplacement `Failed`) vers un ennemi unique : pas de nouveau combat ni de réveil par scan sur lui avant `FailedEnemyCooldown` ; après le délai il est de nouveau pris ; un second ennemi éligible, lui, est pris tout de suite.
- [ ] À portée au moment du coup (sans marche), le yaw du serviteur pointe vers l'ennemi (écart < 1°), à chaque coup ; le roll et le pitch sont inchangés.
- [ ] Ennemi tué : `IsFighting` faux ; s'il en reste un en zone le serviteur le prend, sinon `IsWandering` vrai et `LastAction == Wander`.
- [ ] Serviteur `Unassigned` ou `FieldHarvest` avec un ennemi en zone : jamais `IsFighting`, PV de l'ennemi inchangés ; un `FieldHarvest` en récolte garde `IsHarvesting` vrai quand il est frappé.
- [ ] Un ennemi hors zone est ignoré ; un ennemi qui sort de la zone, ou une dé-assignation, met fin au combat (`IsFighting` faux, plus de dégâts).
- [ ] Soldat dont le déplacement vers un serviteur finit en `Success` hors portée : serviteur non exclu (`ExcludedTargets`), nouvelle demande au tick suivant ; fin `Failed` : exclu comme avant.
- [ ] Soldat engagé sur un serviteur qui s'est déplacé de plus que la portée : une nouvelle demande vers la position actuelle part après `RepathInterval` (pas avant, pas à chaque tick) ; cible destructible fixe : aucune re-demande.

## API exposée
```cpp
// AServant (Servants/Servant.h) : public IDestructible
UHealthComponent* GetHealthComponent_Implementation() const override;
UFUNCTION(BlueprintPure, Category = "Combat") float GetAttackRange() const;
UFUNCTION(BlueprintPure, Category = "Combat") float GetAttackInterval() const;
void Attack(AActor* Target); // ApplyDamage(Target, AttackDamage, GetController(), this, UDamageType)
// AServantAIController
UFUNCTION(BlueprintPure, Category = "Servant|Combat") bool IsFighting() const;
UFUNCTION(BlueprintPure, Category = "Servant|Combat") AActor* GetFightTarget() const; // nullptr hors combat
protected: virtual EPathFollowingRequestResult::Type RequestMoveToEnemy(AActor* Enemy); // MoveToActor
// AEnemySoldier : UFUNCTION(BlueprintPure) bool IsDead() const;
// UEnemyRegistrySubsystem (UWorldSubsystem, C++ seul)
void RegisterEnemy(AEnemySoldier*); void UnregisterEnemy(AEnemySoldier*);
TArray<AEnemySoldier*> GetLivingEnemies() const;
```
`UHealthComponent` du serviteur : `VisibleAnywhere, BlueprintReadOnly`, `private`, `AllowPrivateAccess`.

## Valeurs réglables (placeholders, à lister dans `TODO.md` « À régler »)
- `AServant` (`Combat`, EditAnywhere) : `AttackDamage` 15, `AttackRange` 100, `AttackInterval` 1.2.
- PV : `MaxHealth` du `UHealthComponent` de `BP_Servant`, 100 (défaut du composant).
- `AEnemyAIController` (`Enemy|Combat`) : `RepathInterval` 1.0.
- `AServantAIController` (`Servant|Combat`) : `CombatWalkSpeed` 400, `CombatNextDelay` 0.5, `EnemyScanInterval` 0.5, `FailedEnemyCooldown` 5.0 (non éditables avant le jeu, doc technique section 12).

## Cas limites
- `MoveToActor` finit de façon synchrone (`Failed`, `AlreadyAtGoal`) et `StopMovement` aussi : `bIsFighting` et `bIsMovingToEnemy` écrits avant l'appel ; `EndCombat` termine toujours par un `ScheduleDecision` explicite (sinon plus aucune décision planifiée si aucun déplacement n'était actif).
- Assignation ou ennemi qui entre en zone pendant une marche d'errance : le scan d'ennemis (`EnemyScanInterval`) annule la marche et lance le combat ; plus de limite « pas de réveil » (remplacée par la décision de Timéo ci-dessus). Un serviteur déjà en `Return` ou `Teleport` n'est pas réveillé.
- Serviteur hors zone (`Return`) : ni ciblable par les ennemis (origine hors zone) ni combattant.
- Mort pendant le combat : `StopWandering` coupe `CombatTimer` avant `StopMovement`.
- Dettes `TActorIterator` / `ExcludedTargets` : non touchées (voir hors périmètre).

## Périmètre de fichiers
- **Créer** : `Enemies/EnemyRegistrySubsystem.h/.cpp` ; `Tests/EnemyRegistrySpec.cpp`, `Tests/ServantCombatWorldSpec.cpp`, `Tests/ServantCombatTestHelpers.h` (contrôleur à couture, serviteur de test).
- **Modifier** : `Servants/Servant.h/.cpp`, `Servants/ServantAIController.h/.cpp`, `Enemies/EnemySoldier.h/.cpp`, `Enemies/EnemyAIController.h/.cpp`, un `Tests/Enemy*Spec.cpp` existant (cas `Success` hors portée).
- **Asset** : `BP_Servant` (valeurs par défaut). Pas de `.umap`, `Build.cs` ni `Default*.ini`.
- **Points chauds** (fin de branche, `consistency-keeper`) : `TODO.md` (placeholders, retrait de 2.2c), `documentation-technique.md` (sections 11-12).

## Hors périmètre
- Sorts du joueur, animations de combat et de mort, barre de vie, portée à distance.
- Réservation des ennemis entre serviteurs, interception hors de la zone.
- Dettes `TActorIterator` par tick et `ExcludedTargets` (non bloquantes, à traiter à part).
- Équilibrage chiffré (placeholders ci-dessus).
