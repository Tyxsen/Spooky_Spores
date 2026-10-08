# TODO — Spooky_Spores
 
## Prochaine étape
 
- [x] Panneau de contrôles en lecture seule (affichage des touches actuelles, toggle Entrée, pas de remappage pour l'instant) — terminé, testé, fusionné dans `dev`
- [ ] Composant de santé + frapper les arbres et pierres + premier sort avec mana — sur une nouvelle branche `feature/...` depuis `dev`

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

**Chunk immédiat (~10 jours, avant S2)** — voir « Prochaine étape » ci-dessus

**Semaine de cours (Bloc 1, S2)** : livrable du CDC

**File d'attente après S2, dans l'ordre**
1. [ ] Un soldat ennemi simple : se déplace, attaque, meurt, laisse du butin (réutilise les drops)
2. [ ] Résurrection : potion + cadavre = serviteur (le cadavre implémente `IInteractable`)
3. [ ] Serviteur qui récolte la ressource la plus proche — ferme la boucle de jeu principale
4. [ ] Menu de paramètres complet : son, FOV, remappage réel des touches (Enhanced Input, Player Mappable Keys)
5. [ ] Système de sauvegarde (détail ci-dessous)
6. [ ] UI hors du Character (PlayerController/HUD) + despawn des drops au sol après délai
7. [ ] Vrai inventaire (modèle de données : stacks, slots ; UI liste simple)
8. [ ] UI drag-and-drop sur l'inventaire + barre de raccourcis (9 emplacements)
9. [ ] Objets équipables + contrôles contextuels selon l'objet en main
10. [ ] Roue de sorts (clic droit maintenu) + mana généralisé (4 emplacements, recharge ~5s)
11. [ ] Sort 2 + Sort 3
12. [ ] Sort 4
13. [ ] Vue de gestion top-down
14. [ ] Construction de base (placement, snapping)
15. [ ] Construction ↔ ressources (coût, effets des bâtiments)
16. [ ] Village : disposition + patrouilles de soldats
17. [ ] Village : condition de victoire + clôture narrative

**Semaine de cours (Bloc 1, S3)** : livrable du CDC + oral de fin de Bloc 1 — position exacte dans la file inconnue pour l'instant (nombre de semaines d'alternance restantes non précisé).

À chaque étape significative : tag et Release GitHub (`v0.2`, `v0.3`...).
 
## Système de sauvegarde
 
À concevoir avant les mécaniques de progression. Données à persister :

- [ ] État des objets déplacés (position et rotation des objets saisissables, comme `BP_Rock`)
- [ ] État des objets interactables (nœuds déjà récoltés, drops encore au sol)
- [ ] Narration : `bHasTriggered` de chaque `AStoryTrigger`, pour qu'un texte déjà lu ne réapparaisse pas
- [ ] Ressources du `UResourceCounterComponent`
- [ ] Plus tard : plantations en cours de pousse, serviteurs, potions, état de la base

## Idée validée : frapper les ressources
 
- [ ] Décider avec quoi le nécromancien frappe (bâton, mains, outils, sort)
- [ ] Arbres et pierres avec le composant de santé, via le système de dégâts d'Unreal (`ApplyDamage` / `OnTakeAnyDamage`)
- [ ] À zéro point de vie : apparition des drops avec la logique existante
- [ ] Vibration de l'objet frappé
- Les champs restent récoltés avec E. Ne pas coder de logique de coups spécifique aux arbres avant le composant de santé.

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
- [ ] Effets visuels des sorts (Niagara)
- [ ] Plus de fragments sur le champignon si besoin
- [ ] Matériau réel pour `AFieldPlot` (remplace la sphère de debug colorée par état)

## Idées de gameplay à rediscuter
 
- [ ] Attraction des drops vers le joueur à proximité
- [ ] Fusion des drops identiques proches
- [ ] Repousse des arbres et des pierres (à réfléchir avec la plantation)
- [ ] Interface de sélection de recette pour le chaudron (pertinente à partir d'au moins deux recettes)
- [ ] Idée initiale du chaudron (UI drag-and-drop, inventaire à droite / cases de recette à gauche) mise de côté : dépend d'un vrai système d'inventaire (voir file d'attente, point 7-8)
 