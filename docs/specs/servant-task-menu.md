# Spec — Assignation des tâches aux serviteurs : menu (2.1)
Item TODO : 2.1 Menu (sans comportement derrière) | Branche : feature/servant-task-menu | Statut : brouillon

## Objectif
Le joueur ouvre un menu (Tab) qui met le jeu en pause et affiche deux colonnes (`GDD.md`, « Assignation des tâches ») : les serviteurs non assignés et une section par catégorie. Il affecte un serviteur par clic (serviteur puis catégorie). 2.1 livre le **modèle d'affectation en C++** et le menu qui l'appelle. Aucun comportement nouveau : tout serviteur garde l'errance actuelle, quelle que soit sa tâche.

## Comportement
- `EServantTask` : `Unassigned` (défaut), `AttackDefense`, `FieldHarvest`. Ajouter une catégorie = une valeur d'enum + une section dans le widget.
- Registre `UServantSubsystem` (`UWorldSubsystem`, modèle `UBaseZoneSubsystem`) : `AServant::BeginPlay` s'enregistre, `EndPlay` se retire. C'est la **source unique** de la tâche (un tableau d'entrées {serviteur, tâche, numéro}) : rien n'est stocké sur `AServant`, donc impossible de changer une tâche sans l'événement.
- À l'enregistrement : tâche `Unassigned`, numéro = compteur du monde (1, 2, 3...), jamais réutilisé. Le menu affiche « Serviteur N ».
- `OnAssignmentsChanged` (sans paramètre) est diffusé à chaque enregistrement, retrait ou changement effectif de tâche. Le widget reconstruit tout ; pas de diff (quelques serviteurs).
- `AssignTask` ne touche pas au contrôleur : `AServantAIController` n'est pas modifié en 2.1.
- Logique du menu dans une classe C++ `UServantTaskMenuWidget` (parent de `WBP_ServantTaskMenu`) : sélection, assignation, abonnement au registre. Le Blueprint ne fait que la mise en page et le câblage des clics.
- Le widget **reçoit** le registre via `InitializeMenu` (appelé par `BP_FirstPersonCharacter`, nœud `Get World Subsystem`). Widget créé une fois au `BeginPlay`, masqué, puis affiché ou masqué au toggle (même principe que `WBP_ControlsPanel`).
- Sélection par clic : clic sur un serviteur = sélection (remplace la précédente) ; clic sur une catégorie ou sur la colonne « Non assignés » = `AssignSelectedServant`, puis la sélection est vidée.
- Toggle (Tab) dans le Character, en C++ (`DoToggleServantMenu`). À l'ouverture : `SetPause(true)`, curseur visible, `FInputModeGameAndUI` (`SetHideCursorDuringCapture(false)`), puis l'événement BP `OnServantMenuToggled(true)`. À la fermeture : l'inverse, avec `FInputModeGameOnly`. Une fonction privée `SetServantMenuOpen(bool)` suffit : pas de système de pause générique (un seul consommateur ; la vue de gestion, item 13, pourra la déplacer avec l'UI hors du Character, item 6).
- `FInputModeUIOnly` est exclu : il appelle `SetIgnoreInput(true)` et Tab ne fermerait plus le menu (vérifié dans `PlayerController.cpp`).
- Pendant la pause, deux choses restent actives : `IA_ToggleServantMenu` (seule action avec `bTriggerWhenPaused = true`, vérifié dans `EnhancedPlayerInput.cpp`) et les clics UMG (Slate, indépendants de la pause du monde). Tout le reste est figé : ticks d'acteurs, `FTimerManager` (décision d'errance, chaudron...), physique, autres Input Actions (vérifié dans `LevelTick.cpp`).
- Boutons du menu en `IsFocusable = false`, pour que le focus clavier reste au viewport et que Tab aille à l'Enhanced Input plutôt qu'à la navigation Slate (à confirmer en PIE).

## Critères d'acceptation
- [ ] Un serviteur spawné est enregistré : `GetServantCount` +1, `GetServantTask` = `Unassigned`, numéro = précédent + 1 ; un serviteur détruit est retiré de toutes les listes et son numéro n'est pas réattribué.
- [ ] `AssignTask(S, FieldHarvest)` renvoie `true`, `GetServantTask(S)` = `FieldHarvest`, et S apparaît dans `GetServantsWithTask(FieldHarvest)` et dans aucune autre liste.
- [ ] `OnAssignmentsChanged` est diffusé exactement une fois par enregistrement, par retrait et par `AssignTask` qui change la tâche.
- [ ] Réassigner la même tâche renvoie `false`, sans diffusion ; `AssignTask(S, Unassigned)` depuis une catégorie renvoie `true` et remet S dans la liste `Unassigned`.
- [ ] `AssignTask` sur `nullptr` ou sur un serviteur non enregistré renvoie `false`, sans diffusion ni changement d'état.
- [ ] `GetServantsWithTask` renvoie les serviteurs dans l'ordre d'enregistrement ; sans aucun serviteur, la liste de chaque tâche est vide.
- [ ] Après `AssignTask` vers n'importe quelle tâche, le contrôleur du serviteur reste en errance (`IsWandering() == true`).
- [ ] Widget : `AssignSelectedServant` sans sélection renvoie `false` ; avec une sélection, il assigne puis vide la sélection ; la sélection est vidée si le serviteur sélectionné est retiré du registre ; `OnMenuRefreshRequested` est appelé par `InitializeMenu` puis à chaque diffusion.
- [ ] PIE : Tab ouvre le menu (jeu en pause, curseur visible) ; Tab le referme (jeu repris, curseur masqué, regard souris rétabli). Menu ouvert : ZQSD, souris, E, L, clic gauche, clic droit et Entrée sont sans effet.
- [ ] PIE : un clic sur un serviteur puis sur « Récolte de champ » le fait changer de colonne ; l'affectation est conservée après fermeture et réouverture.

## API exposée
```cpp
UENUM(BlueprintType) enum class EServantTask : uint8 { Unassigned UMETA(DisplayName="Non assigné"),
  AttackDefense UMETA(DisplayName="Attaque / défense"), FieldHarvest UMETA(DisplayName="Récolte de champ") };
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnServantAssignmentsChanged);
// UServantSubsystem (Category = "Servant|Task")
UFUNCTION(BlueprintCallable) bool AssignTask(AServant* Servant, EServantTask NewTask); // true only if the task changed
UFUNCTION(BlueprintPure) EServantTask GetServantTask(const AServant* Servant) const;    // Unassigned if not registered
UFUNCTION(BlueprintPure) TArray<AServant*> GetServantsWithTask(EServantTask Task) const; // registration order
UFUNCTION(BlueprintPure) int32 GetServantNumber(const AServant* Servant) const;          // 0 if not registered
UFUNCTION(BlueprintPure) int32 GetServantCount() const;
UFUNCTION(BlueprintPure) bool IsServantRegistered(const AServant* Servant) const;
UPROPERTY(BlueprintAssignable) FOnServantAssignmentsChanged OnAssignmentsChanged;
void RegisterServant(AServant*); void UnregisterServant(AServant*); // C++ only, AServant BeginPlay/EndPlay; duplicates/unknown ignored
// UServantTaskMenuWidget : UUserWidget, UCLASS(Abstract) (Category = "Servant|TaskMenu")
UFUNCTION(BlueprintCallable) void InitializeMenu(UServantSubsystem* InSubsystem); // rebinds if called twice, then refresh
UFUNCTION(BlueprintCallable) void SelectServant(AServant* Servant);
UFUNCTION(BlueprintCallable) bool AssignSelectedServant(EServantTask Task);
UFUNCTION(BlueprintPure) AServant* GetSelectedServant() const;
UFUNCTION(BlueprintPure) UServantSubsystem* GetServantSubsystem() const;
UFUNCTION(BlueprintImplementableEvent) void OnMenuRefreshRequested();               // protected: rebuild both columns
UFUNCTION(BlueprintImplementableEvent) void OnSelectionChanged(AServant* NewSelection); // protected: highlight
// ASpooky_SporesCharacter
UPROPERTY(EditAnywhere, Category = "Input") UInputAction* ToggleServantMenuAction = nullptr;
UFUNCTION(BlueprintImplementableEvent, Category = "UI") void OnServantMenuToggled(bool bIsOpen);
UFUNCTION(BlueprintPure, Category = "UI") bool IsServantMenuOpen() const;
```

## Valeurs réglables
Aucune. Touche : Tab (`IMC_Default`), provisoire (refonte des touches prévue par Timéo).

## Cas limites
- Serviteur détruit alors qu'il est assigné ou sélectionné : retiré du registre, diffusion, sélection vidée, aucune entrée fantôme dans les listes.
- Menu ouvert sans aucun serviteur : colonnes vides, aucune erreur ; un clic sur une catégorie sans sélection ne fait rien.
- Character non possédé par un `APlayerController` : `DoToggleServantMenu` ne fait rien (log `Warning`), sans changer d'état.
- Maj (sprint) maintenue à l'ouverture puis relâchée pendant la pause : le sprint ne doit pas rester bloqué après la fermeture (à vérifier en PIE).

## Périmètre de fichiers
- **C++, à créer** : `Servants/ServantTask.h` (enum), `Servants/ServantSubsystem.h/.cpp`, `Servants/ServantTaskMenuWidget.h/.cpp`.
- **C++, à modifier** : `Servants/Servant.h/.cpp` (`BeginPlay`/`EndPlay`), `Core/Character/Spooky_SporesCharacter.h/.cpp` (action, toggle, pause, événement).
- **Tests (`qa-test-writer`)** : `Tests/ServantSubsystem*Spec.cpp`, `Tests/ServantTaskMenuWidget*Spec.cpp` (noms libres).
- **[MCP]** : créer `Core/Input/Actions/IA_ToggleServantMenu` (Digital, `bTriggerWhenPaused = true`), mapper Tab dans `IMC_Default`, renseigner `ToggleServantMenuAction` sur `BP_FirstPersonCharacter`, vérifier qu'aucune autre IA n'a `bTriggerWhenPaused` à `true` et que Tab n'est pas déjà mappé.
- **[HUMAIN]** (le MCP ne crée ni widget ni Event Graph) : `UI/Widgets/WBP_ServantTaskMenu` (parent `UServantTaskMenuWidget`, deux colonnes, boutons non focusables), `UI/Widgets/WBP_ServantEntry` (bouton « Serviteur N » → `SelectServant`), Event Graph de `BP_FirstPersonCharacter` (création au `BeginPlay`, `InitializeMenu`, visibilité sur `OnServantMenuToggled`).
- **Points chauds** : aucun dans le code (`Build.cs` contient déjà `UMG`). `IMC_Default` et `BP_FirstPersonCharacter` sont des assets partagés : à sérialiser. `Lvl_Principal.umap` n'est pas touché. Docs (`documentation-technique.md`, `TODO.md`) : `consistency-keeper` en fin de branche.

## Hors périmètre
- Comportement « attaque / défense ». Intention de Timéo, sans spécification ici : en errance tant qu'aucun ennemi n'est entré dans la base, puis ils vont l'attaquer (dépend de l'item 3).
- Comportement « récolte de champ » (2.2, avec le despawn des drops au sol de la dette technique).
- Glisser-déposer (item 8 : seul le widget changera, l'API C++ reste).
- Sauvegarde des affectations (item 5), UI hors du Character (item 6), vue de gestion et pause associée (item 13).
