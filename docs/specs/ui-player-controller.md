# Spec — UI hors du Character (PlayerController propriétaire des widgets)
Item TODO : 6 (partie 6a) | Branche : feature/ui-player-controller | Statut : brouillon

## Objectif
Timéo : « il faut un système modulaire qui me permet de rajouter simplement de l'ui à mon jeu ». Le PlayerController (qui survit au changement de pawn) possède et pilote tous les widgets ; ajouter une UI = ajouter une entrée dans un tableau, sans C++ ni toucher au Character. Premier consommateur futur : menu de paramètres (item 4a). Le despawn des drops est dans `drop-despawn.md` (6b).

## Comportement
- `FUIWidgetEntry` (USTRUCT, `UI/UIWidgetEntry.h`) : `Id` (FName, unique), `WidgetClass`, `ZOrder` (int32, 0), `bOpenAtStart` (false), `bPausesGame` (false), `ToggleAction` (`UInputAction*`, optionnel).
- `ASpooky_SporesPlayerController::UIEntries` (`TArray<FUIWidgetEntry>`, EditAnywhere). Au `BeginPlay` d'un contrôleur local, chaque entrée valide est créée (`CreateWidget`, outer = le contrôleur) et ajoutée à l'écran (`AddToPlayerScreen(ZOrder)`) une seule fois. Ouvert = `Visible`, fermé = `Collapsed` (pas de `RemoveFromParent` : l'état et les abonnements du widget survivent). Entrée sans `WidgetClass`, `Id` vide ou doublon : `UE_LOG(Error)` et entrée ignorée.
- API de pilotage : `OpenUI/CloseUI/ToggleUI(Id)`, `IsUIOpen(Id)`, `GetUIWidget(Id)`. `Id` inconnu : `false`/`nullptr` + `UE_LOG(Warning)`. Ouvrir un UI déjà ouvert (ou fermer un fermé) : no-op, aucun évènement.
- **Mode modal** (remplace `Character::SetServantMenuOpen`) : si au moins une UI ouverte a `bPausesGame`, le jeu est en pause, curseur visible, `FInputModeGameAndUI` (`SetHideCursorDuringCapture(false)`) ; sinon dépause, curseur masqué, `FInputModeGameOnly`. Les trois appels moteur sont isolés dans `ApplyModalState(bool)` (protégée, virtuelle), appelée **uniquement au changement** d'état modal (pas à chaque ouverture). Pas de `FInputModeUIOnly` (`CLAUDE.md` section 8). Plusieurs UI pausantes ouvertes en même temps : permis, pas de pile ; la pause tombe à la fermeture de la dernière.
- **Entrée** : si `ToggleAction` est renseignée, le contrôleur lie `ETriggerEvent::Started` à `ToggleUI(Id)` dans `SetupInputComponent`. L'IA d'un UI pausant garde `bTriggerWhenPaused`. Les touches de **gameplay** restent dans le Character (E, tir, sprint...).
- **Dépendances** : interface `IPlayerUIWidget` (`UI/PlayerUIWidget.h`), `BlueprintNativeEvent InitializeForPlayer(APlayerController* Controller, APawn* Pawn)`. `Controller` toujours valide, `Pawn` nul si aucun pawn possédé. Appelée sur chaque widget créé qui l'implémente (`Implements<UPlayerUIWidget>()`) : à la création, puis à chaque `OnPossess` (nouveau pawn) et `OnUnPossess` (`Pawn = nullptr`). Le widget **doit être idempotent** (se désabonner puis se réabonner). Le widget reçoit le pawn et en extrait lui-même le composant (barre de stamina : `UStaminaComponent`, compteur : `UResourceCounterComponent`), jamais de recherche dans le monde, **sauf exception assumée** : `WBP_CauldronProgress` (Blueprint) fait `GetActorOfClass(BP_Cauldron)` dans son `InitializeForPlayer`, puis `InitializeCauldronProgress(chaudron)` (migration à l'identique ; limite acceptée : un seul chaudron dans le niveau, remplacement par un `UWorldSubsystem` à l'item 15).
- `UServantTaskMenuWidget` implémente `IPlayerUIWidget` : `InitializeForPlayer_Implementation` appelle `InitializeMenu(GetWorld()->GetSubsystem<UServantSubsystem>())` ; `InitializeMenu` et sa Spec ne changent pas.
- Évènement : `FOnUIOpenChanged(FName Id, bool bIsOpen)`, `BlueprintAssignable`, émis à chaque changement réel (remplace `OnServantMenuToggled`).
- Le Character perd : `ToggleControlsAction`, `ToggleServantMenuAction`, `OnToggleControlsPanel`, `OnServantMenuToggled`, `DoToggleControlsPanel`, `DoToggleServantMenu`, `SetServantMenuOpen`, `bIsServantMenuOpen`, `IsServantMenuOpen`.
- Choix techniques : pas de HUD (`AHUD`) ni de composant dédié, tout dans le contrôleur (une seule classe consommatrice) ; couche = `ZOrder` seul ; contrôleur non local (test) : pas de création.
- Conception = minimum suffisant pour « modulaire » : une struct, un tableau, une interface, 5 fonctions. Hors besoin, donc écartés : pile de menus, gestion de focus, animations.

## Critères d'acceptation
- [ ] Une entrée valide produit un widget (`GetUIWidget(Id)` non nul), créé une seule fois même après plusieurs `OpenUI/CloseUI`.
- [ ] `bOpenAtStart` : `IsUIOpen` vrai juste après `BeginPlay` pour ces entrées, faux pour les autres ; visibilité `Visible` / `Collapsed` en cohérence.
- [ ] Entrée invalide (classe nulle, `Id` vide, doublon) : ignorée, `GetUIWidget` nul, les entrées valides voisines restent créées.
- [ ] `OpenUI` / `CloseUI` / `ToggleUI` changent `IsUIOpen` et émettent `OnUIOpenChanged` exactement une fois par changement réel, jamais sur un no-op ; `Id` inconnu : retour négatif sans évènement.
- [ ] Ouvrir une UI `bPausesGame` appelle `ApplyModalState(true)` une fois ; en ouvrir une seconde ne le rappelle pas ; fermer la première non plus ; fermer la dernière appelle `ApplyModalState(false)`.
- [ ] Une UI non pausante (ex. panneau de contrôles) ouverte ou fermée n'appelle jamais `ApplyModalState`.
- [ ] `ApplyModalState` réel : après `true`, `UGameplayStatics::IsGamePaused` vrai et curseur visible ; après `false`, l'inverse (test de monde si le pause est possible dans `FTestWorldWrapper`, sinon non automatisé et listé).
- [ ] `InitializeForPlayer` est appelé à la création avec le pawn courant, puis à `OnPossess(NouveauPawn)` avec ce pawn, puis à `OnUnPossess` avec `nullptr` ; un widget qui n'implémente pas l'interface n'est pas touché.
- [ ] `UServantTaskMenuWidget::InitializeForPlayer` branche le menu sur le `UServantSubsystem` du monde (`GetServantSubsystem()` non nul), et le rappeler deux fois ne double pas les abonnements (une diffusion `OnAssignmentsChanged` = un seul `OnMenuRefreshRequested`).
- [ ] `Character` ne déclare plus aucune des fonctions listées ci-dessus (vérifié par compilation) et `ServantTaskMenuWidgetSpec` reste verte.

## API exposée
- `USTRUCT(BlueprintType) struct FUIWidgetEntry` : `UPROPERTY(EditAnywhere, BlueprintReadWrite)` pour chaque champ ci-dessus.
- `UFUNCTION(BlueprintCallable, Category="UI") bool OpenUI(FName Id)`, idem `CloseUI`, `ToggleUI` (vrai si l'état a changé) ; `UFUNCTION(BlueprintPure, Category="UI") bool IsUIOpen(FName Id) const` ; `UFUNCTION(BlueprintPure, Category="UI") UUserWidget* GetUIWidget(FName Id) const`.
- `UPROPERTY(BlueprintAssignable, Category="UI") FOnUIOpenChanged OnUIOpenChanged`.
- `UINTERFACE(BlueprintType) UPlayerUIWidget` / `IPlayerUIWidget::InitializeForPlayer` (`BlueprintNativeEvent, BlueprintCallable`, sans `_Implementation` dans l'interface).
- Protégées virtuelles (coutures de test) : `UUserWidget* CreateUIWidget(const FUIWidgetEntry&)` (création + ajout à l'écran), `void ApplyModalState(bool bModal)`.

## Valeurs réglables
`UIEntries` : vide par défaut (renseignée dans le BP enfant). Par entrée : `ZOrder` 0, `bOpenAtStart` false, `bPausesGame` false, `ToggleAction` nulle.

## Cas limites
- `BeginPlay` du contrôleur peut précéder la possession : le widget reçoit alors `Pawn = nullptr`, puis le pawn à `OnPossess` (d'où l'idempotence).
- UI `bOpenAtStart` + `bPausesGame` : l'état modal est appliqué dès le `BeginPlay`.
- Contrôleur détruit pendant la pause (fin de PIE) : rien à nettoyer côté code ; la pause du monde disparaît avec lui.
- Un `ToggleAction` partagé par deux entrées : les deux basculent (permis, pas de garde).

## Périmètre de fichiers
- **Créer** : `UI/UIWidgetEntry.h`, `UI/PlayerUIWidget.h`, `UI/PlayerUIWidget.cpp` (vide), `Tests/PlayerUIControllerSpec.cpp`, `Tests/PlayerUITestHelpers.h` (contrôleur de test dérivé, surcharge les deux coutures).
- **Modifier** : `Core/Controller/Spooky_SporesPlayerController.{h,cpp}`, `Core/Character/Spooky_SporesCharacter.{h,cpp}`, `Servants/ServantTaskMenuWidget.{h,cpp}`.
- **Points chauds** : aucun (`Build.cs` inchangé, `UMG` et `EnhancedInput` déjà présents ; à confirmer par l'implémenteur). Docs partagées : mises à jour par `consistency-keeper` en fin de branche.
- **Éditeur (hors C++, `bp-integration-planner`)** : `BP_FirstPersonPlayerController` (existe) : remplir `UIEntries` (ControlsPanel, ServantMenu `bPausesGame` + `IA_ToggleServantMenu`, CauldronProgress, StaminaBar, ResourceCounter, `bOpenAtStart` pour les trois HUD). Vérifier que `BP_FirstPersonGameMode` utilise ce contrôleur. `BP_FirstPersonCharacter` : retirer les 5 `Create Widget`, les évènements `OnToggleControlsPanel`/`OnServantMenuToggled` et leurs valeurs `ToggleControlsAction`/`ToggleServantMenuAction` (sinon erreurs de compilation Blueprint). Ajouter `IPlayerUIWidget` aux WBP stamina/compteur et rebrancher leurs sources via le pawn reçu. `Lvl_Principal.umap` non touchée.

## Hors périmètre
- Menu de paramètres (4a), remappage (4b), drag-and-drop (8), vue top-down (13).
- Widget mobile existant (`MobileControlsWidget`), `WBP_StoryText` (non créé par le Character).
- Refonte des Input Mapping Contexts (item 9) ; pause générique au-delà de ce besoin.

## Points d'incertitude
- Chaudron (résolu par Timéo) : « après la création du widget, je fais un get actor of class "BP_Cauldron" et j'appelle ensuite la fonction Initialize Cauldron Progress qui vient du wbp cauldron progress [...] avec le chaudron que j'ai eu en retour en tant que reference, comme ça les deux sont liés. » Le quand-s'affiche reste à relever par le `bp-integration-planner` dans `BP_FirstPersonCharacter`.
- `FTestWorldWrapper` : pause et `SetInputMode` peuvent être sans effet sans GameMode ni LocalPlayer (d'où les coutures) ; à confirmer par `qa-test-writer`.
- Écart avec la consigne de l'orchestrateur : l'entrée de bascule vit dans l'entrée d'UI (donc le contrôleur), pas dans le Character, sinon chaque nouvelle UI imposerait du C++ dans le Character.
