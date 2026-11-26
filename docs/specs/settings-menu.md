# Spec — Menu de paramètres (FOV seul, extensible)
Item TODO : 4a Menu de paramètres | Branche : feature/settings-menu | Statut : validée par Timéo (3 points acceptés tels que proposés)

## Objectif
Le joueur règle son FOV dans un menu, le réglage survit aux sessions. Le but réel est l'**extensibilité** (Timéo : « je veux être sûr que ça soit modulable et nickel pour rajouter d'autres options plus tard même si dans cette partie il n'y a que le FOV »). Premier consommateur du foyer d'UI de l'item 6 (`UIEntries` + `IPlayerUIWidget`). Hors périmètre : son, remappage (4b).

## Comportement
- **Déclarer un réglage** = une entrée `FSettingDefinition` (`Id`, `DisplayName`, `Type`, `Min`, `Max`, `Default`, `Step`) dans `SettingsCatalog` (seul endroit). Les valeurs sont **toutes stockées en float** ; `ESettingType` ne décrit que la présentation (aujourd'hui `Float` seul ; un futur `Toggle` = 0/1, `Choice` = index : une valeur d'enum + une classe de ligne, sans toucher au stockage). Pas de variant : KISS.
- **Application** : ne peut pas être de la donnée sans sur-abstraire. Le consommateur (ici le Character) lit `GetFloat(Id)` au démarrage et écoute `OnSettingChanged`. Ajouter un réglage = 1 entrée de catalogue + 1 ligne chez son consommateur ; zéro changement dans le widget, le sous-système ou la sauvegarde.
- **Stockage** : `USettingsSubsystem` (`UGameInstanceSubsystem`, vit pendant tout le processus, survit aux niveaux et aux pawns). `SetFloat` borne dans `[Min, Max]`, mémorise, diffuse. Aucune écriture disque dans `SetFloat`.
- **Persistance (tranché)** : `USaveGame` dédié `USettingsSaveGame { TMap<FName,float> Values }`, slot `"Settings"`, appels synchrones (fichier minuscule). Vérifié dans `GameplayStatics.h` (UE 5.8) : `SaveGameToSlot` / `LoadGameFromSlot` / `DoesSaveGameExist`. Écriture : au relâchement du slider, et à `Deinitialize` si modifié (filet de sécurité, pas de perte à la fermeture).
  - Contre `UGameUserSettings` (`config=GameUserSettings`, `GameUserSettings.h`) : il faudrait un `UPROPERTY(config)` par réglage (donc toucher la classe de persistance à chaque ajout, ce que le TODO interdit) et une ligne `GameUserSettingsClassName` dans `DefaultEngine.ini` (point chaud). Il reste le bon outil pour la vidéo (résolution, vsync) si on en ajoute un jour : décision à rouvrir alors.
  - **Séparation avec l'item 5b** : slot « Settings » distinct des slots de partie (`Save_<n>`). Le FOV est une préférence de la machine, pas un état du monde : charger ou supprimer une partie ne doit pas le changer. Même API que 5b, donc rien de plus à apprendre.
- **Chargement = frontière de confiance** (fichier modifiable) : à `Initialize`, pour chaque définition, valeur absente, NaN ou infinie → `Default` ; sinon bornée. Clé inconnue ignorée. Slot absent = premier lancement (Log, pas d'erreur) ; slot illisible = Warning + défauts. Définition invalide (`Id` vide ou doublon, `Min >= Max`, `Default` hors bornes) : ignorée, `Error` loguée.
- **FOV** : le Character est le **seul** à écrire `FirstPersonCameraComponent->SetFieldOfView` (réglage = propriété `FieldOfView`, défaut moteur 90, cohérent avec le README). Il l'applique à `BeginPlay` (couvre démarrage et réapparition : un nouveau pawn relit le réglage) et sur `OnSettingChanged` (filtre sur l'Id), et se désabonne à `EndPlay`. Aucun widget ne touche la caméra. Vérifié par recherche dans `Source/` : aucun autre code n'écrit le FOV (le sprint ne le touche pas). `FirstPersonFieldOfView` (70, bras seuls) reste fixe.
- **Menu** : `USettingsMenuWidget` (`Abstract`, `IPlayerUIWidget`) : `InitializeForPlayer` récupère le sous-système via `Controller->GetGameInstance()` (le widget reçoit ses dépendances, il ne cherche pas dans le monde) puis **reconstruit** les lignes (idempotent : vide `RowsContainer`, recrée). Une ligne par définition valide, classe choisie par `RowClasses` (`TMap<ESettingType, TSubclassOf<USettingRowWidget>>`) ; type sans classe = `Error`, ligne sautée.
- **Ligne** : `USettingRowWidget` (`Abstract`, `SetupRow(Subsystem, Definition)`) et `USettingFloatRowWidget` (slider borné par la définition, valeur affichée). La ligne écoute `OnSettingChanged` (se resynchronise, sans boucle : une valeur inchangée ne rediffuse pas) et se désabonne en `NativeDestruct`.
- **Ouverture** : entrée `Settings` de `UIEntries`, `bPausesGame = true` : réutilise `SetUIOpen` / `ApplyModalState` (`FInputModeGameAndUI`, jamais `UIOnly`, piège CLAUDE.md §8). Touche **provisoire `O`** (« au pif » selon Timéo, refaite avec l'item 9) : libre d'après CLAUDE.md §6 (ni `²`, Échap, Tab, Entrée, ZQSD, E, L, Espace, Maj). L'`IA` a `bTriggerWhenPaused` pour pouvoir fermer.

## Critères d'acceptation
- [ ] Sans slot, `GetFloat(FOV)` = `Default` (90).
- [ ] `SetFloat(FOV, 500)` donne `Max`, `SetFloat(FOV, -5)` donne `Min` ; renvoie vrai.
- [ ] `SetFloat` sur un `Id` inconnu renvoie faux, ne diffuse rien et ne crée aucune entrée.
- [ ] `OnSettingChanged(Id, valeur bornée)` est diffusé une fois par changement réel, jamais si la valeur bornée est égale à l'actuelle.
- [ ] Aller-retour : `SetFloat` + `SaveSettings`, nouvelle instance du sous-système sur le même slot : même valeur ; avec `SetFloat` seul puis `Deinitialize` : valeur aussi relue.
- [ ] Slot forgé avec FOV = NaN, `+inf`, 9999, ou clé absente, plus une clé inconnue : relus respectivement en `Default`, `Default`, `Max`, `Default` ; clé inconnue ignorée sans erreur.
- [ ] Extensibilité : sous-système initialisé avec un catalogue de test contenant un 2e réglage (`Id` autre, bornes autres) : bornage, diffusion et aller-retour fonctionnent sans modifier le sous-système, ni la sauvegarde.
- [ ] Définitions invalides (`Min >= Max`, `Default` hors bornes, `Id` doublon ou vide) : ignorées, les valides restent utilisables.
- [ ] Character : à `BeginPlay`, `FieldOfView` de la caméra = valeur du réglage ; `SetFloat(FOV, x)` ensuite le met à `x` ; un Character créé après (réapparition) reprend la valeur stockée ; après `EndPlay`, plus aucune réaction.
- [ ] Menu : `InitializeForPlayer` crée autant de lignes que de définitions valides, deux appels de suite en laissent le même nombre ; un type sans `RowClass` saute seulement sa ligne ; bouger le slider d'une ligne change `GetFloat`, et `SetFloat` extérieur bouge le slider.

## API exposée
```cpp
// SettingsCatalog.h
UENUM(BlueprintType) enum class ESettingType : uint8 { Float = 0 };   // à compléter en fin uniquement
USTRUCT(BlueprintType) struct FSettingDefinition { FName Id; FText DisplayName; ESettingType Type = ESettingType::Float;
  float Min = 0.f; float Max = 1.f; float Default = 0.f; float Step = 0.f; /* 0 = continu */ };   // UPROPERTY EditAnywhere/BlueprintReadWrite
namespace SettingsCatalog { extern const FName FOV; const TArray<FSettingDefinition>& GetDefinitions(); }
// SettingsSubsystem.h : UCLASS() class USettingsSubsystem : public UGameInstanceSubsystem
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSettingChanged, FName, Id, float, Value);
UFUNCTION(BlueprintPure, Category="Settings") float GetFloat(FName Id) const;       // inconnu : 0 + Warning
UFUNCTION(BlueprintCallable, Category="Settings") bool SetFloat(FName Id, float Value);
UFUNCTION(BlueprintCallable, Category="Settings") void ResetToDefault(FName Id);     // passe par SetFloat
UFUNCTION(BlueprintCallable, Category="Settings") bool SaveSettings();               // vrai si écrit
UFUNCTION(BlueprintPure, Category="Settings") TArray<FSettingDefinition> GetDefinitions() const;
UPROPERTY(BlueprintAssignable) FOnSettingChanged OnSettingChanged;
void InitializeFromDefinitions(const TArray<FSettingDefinition>&, const FString& SlotName); // C++, appelée par Initialize ; seam de test
// SettingsMenuWidget.h : BindWidget UPanelWidget* RowsContainer ; UPROPERTY(EditAnywhere) TMap<ESettingType, TSubclassOf<USettingRowWidget>> RowClasses
// SettingRowWidget.h : USettingFloatRowWidget : BindWidget USlider* ValueSlider, UTextBlock* NameText, UTextBlock* ValueText
// Character : UFUNCTION() private void HandleSettingChanged(FName Id, float Value); virtual void EndPlay(...) override
```

## Valeurs réglables
`FOV` (Id) : `Min` 60, `Max` 120, `Default` 90, `Step` 1 (bornes = **placeholders**, le GDD n'en fixe pas ; à valider). Slot `"Settings"`. `ZOrder` de l'entrée : 12 (au-dessus de `ServantMenu` 11).

## Cas limites
- Pause : la caméra n'est pas mise à jour en pause (`LevelTick.cpp`, `UpdateCameraManager` sauté) : le FOV choisi n'est **visible qu'à la reprise** (le réglage est bien appliqué tout de suite).
- Instance `BP_FirstPersonCharacter` ou `BP_...Controller` modifiée à la main (`FieldOfView` surchargé, `UIEntries`) : l'ignorer masquerait le réglage (CLAUDE.md §8) ; à vérifier sur l'instance.
- `Deinitialize` pendant la fin de PIE : l'écriture ne doit pas échouer ni lever d'erreur si rien n'a changé (pas d'écriture si non modifié).
- Un autre menu pausant déjà ouvert : comportement existant de `SetUIOpen` (modal conservé), pas de cas spécial.

## Périmètre de fichiers
- **Créer** `Source/Spooky_Spores/Settings/` : `SettingsCatalog.h/.cpp`, `SettingsSaveGame.h`, `SettingsSubsystem.h/.cpp`, `SettingsMenuWidget.h/.cpp`, `SettingRowWidget.h/.cpp` (9 fichiers, un dossier = un domaine, pas de sous-dossier). `Tests/SettingsSubsystemSpec.cpp`, `Tests/SettingsWidgetSpec.cpp`.
- **Modifier** `Core/Character/Spooky_SporesCharacter.h/.cpp` (FOV, `EndPlay`). Le contrôleur C++ est inchangé (la réutilisation suffit).
- **Éditeur** (`bp-integrator`) : `IA_ToggleSettings` (booléen, `bTriggerWhenPaused`) ajoutée à `IMC_Default` (touche `O`) ; `WBP_SettingRow_Float` (parent `USettingFloatRowWidget`, widgets nommés `NameText`, `ValueText`, `ValueSlider`) ; `WBP_SettingsMenu` (parent `USettingsMenuWidget`, `VerticalBox` nommée `RowsContainer`, `RowClasses[Float]` = `WBP_SettingRow_Float`) ; entrée `Settings` dans `UIEntries` de `BP_FirstPersonPlayerController` (`bPausesGame`, `ZOrder` 12, `ToggleAction`). Tout le reste est en C++.
- **Points chauds** : aucun touché par le code (`Build.cs` : `UMG`/`Slate`/`Engine` déjà présents ; pas de `.ini`). `TODO.md`, `documentation-technique.md` mis à jour en fin de branche par `consistency-keeper` ; `README.md` (touche `O`) à signaler.

## Validé par Timéo
- Pause sans aperçu en direct → je propose d'accepter (cohérent avec Entrée et Tab, FOV visible à la reprise) ; sinon `bShouldPerformFullTickWhenPaused` sur le contrôleur, effets de bord non évalués → qu'en penses-tu ?
- Bornes FOV 60-120, défaut 90 : le GDD n'en dit rien → je propose ces valeurs → qu'en penses-tu ?
- Pas de bouton Fermer : seule la touche `O` ferme le menu → je propose de l'accepter tant que l'item 9 n'a pas refait les contrôles → qu'en penses-tu ?

## Hors périmètre
- Son, remappage, vidéo (résolution, vsync), FOV des bras, bouton « Fermer » / « Réinitialiser » (la touche `O` ferme), sauvegarde de partie (5a/5b), types `Toggle`/`Choice`.
