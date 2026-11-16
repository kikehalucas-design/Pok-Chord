# PokéChord : générateur d'accords (VST3) pour Ableton

Tu choisis un **style** (House, Deep House, Afro House, Disco / Funk, Jazz, Neo-Soul, Lo-Fi, R&B, Pop, Techno), une **humeur** (Dansant, Joyeux, Énergique, Romantique, Planant, Nostalgique, Mélancolique, Triste, Sombre, Mystérieux, Tendu), et le plugin invente une suite d'accords. Chaque clic sur la Poké Ball en donne une nouvelle.

## Comment récupérer les accords dans Ableton

Ableton ne permet pas à un plugin VST d'envoyer du MIDI vers une autre piste. Le plugin passe donc par un **clip MIDI** :

1. Mets PokéChord sur une piste (c'est maintenant un **effet audio** : pose-le sur une **piste audio**, dans Plug-ins > Audio Effects). Clique sur **la Poké Ball** : tu entends la suite d'accords tout de suite.
2. Quand une suite te plaît, **attrape la zone en pointillés "GLISSE LE MIDI DANS ABLETON"** et glisse-la sur une **piste MIDI** (ou une zone vide d'Ableton pour créer une piste). Le clip contient les notes, tu peux les modifier comme n'importe quel clip.
3. Mets ton propre synthé ou ton piano sur cette piste MIDI : les accords jouent avec ton son.
4. Si le glisser-déposer ne marche pas, utilise le bouton **ENREGISTRER .MID**, puis glisse le fichier depuis le Finder dans Ableton.

Quand tu es sur la piste MIDI avec ton propre son, tu peux couper "SON D'ÉCOUTE" ou retirer le plugin.

## Obtenir le plugin (GitHub)

1. Dans ton dépôt GitHub, ajoute le dossier `PokeChord` (le même principe que pour Minimal Transformer).
2. Ouvre `.github/workflows/main.yml`, remplace tout son contenu par celui du fichier `build.yml` de ce zip, puis **Commit changes**.
3. Onglet **Actions** : la compilation tourne (10 à 15 minutes), puis dans **Artifacts** télécharge `PokeChord-macOS-VST3` (ou Windows).

## Installer (Mac)

Décompresse l'archive téléchargée, puis dans le Terminal :

```
S="$(find ~/Downloads -maxdepth 4 -type d -exec test -f '{}/Contents/MacOS/PokeChord' \; -print | head -1)"; D=~/Library/Audio/Plug-Ins/VST3/PokeChord.vst3; [ -n "$S" ] && mkdir -p ~/Library/Audio/Plug-Ins/VST3 && rm -rf "$D" && cp -R "$S" "$D" && find "$D" -name .DS_Store -delete && chmod +x "$D/Contents/MacOS/PokeChord" && xattr -cr "$D" && codesign --force --deep -s - "$D" && echo "Installé, relance Ableton" || echo "Échec"
```

Puis dans Ableton : ferme et rouvre, **Live > Réglages > Plug-ins**, **Rescanner** (avec la touche Option pour un scan complet).

## Les réglages

- **Style / Humeur / Tonalité** : "Aléatoire" laisse le plugin choisir. Le mode majeur ou mineur dépend de l'humeur.
- **Durée d'un accord** : 1/2 mesure, 1 mesure ou 2 mesures. **Accords** : de 2 à 8.
- **Complexité** : accords simples (triades) vers accords riches (7e, 9e, 11e, 13e).
- **Audace** : ajoute des dominantes secondaires, des substitutions et des emprunts à l'autre mode.
- **Humanisation** : petites variations de vélocité et léger décalage des notes.
- **Rythme** : tenu, offbeat house, syncopé, stabs funk, arpèges, etc.
- **Disposition** : serrée, ouverte (drop 2) ou piano (main gauche + main droite). **Octave** : monte ou descend le registre. **Basse** : ajoute la fondamentale dans le grave.
- **◀** : revient à la suite précédente. **▶** : relance la lecture dans le plugin (elle suit aussi le transport d'Ableton).

La graine est sauvegardée avec ton projet : en rouvrant le set, tu retrouves exactement les mêmes accords.


Chaque humeur a son Pokémon et la couleur de son type : Dansant (Salamèche), Joyeux (Évoli), Énergique (Pikachu), Romantique (Rondoudou), Planant (Ronflex), Nostalgique (Bulbizarre), Mélancolique (Carapuce), Triste (Mewtwo), Sombre (Ectoplasma), Mystérieux (Mew), Tendu (Dracaufeu). 

## L'onglet ANALYSER

Clique sur **ANALYSER** en haut à droite : Noctali écoute l'audio qui passe sur la piste et affiche l'accord entendu, les notes détectées et la liste des accords au fil du morceau.

1. Mets PokéChord sur la piste audio à analyser (une piste d'accords, un sample, un morceau) et lance la lecture.
2. L'accord apparaît en grand, avec les notes entendues. Un accord est ajouté à la liste quand il reste stable un instant.
3. Attrape **GLISSE LES ACCORDS EN MIDI DANS ABLETON** et glisse-le sur une piste MIDI pour récupérer les accords détectés en clip MIDI.
4. **PAUSE / ÉCOUTER** arrête ou relance l'écoute, **EFFACER** vide la liste.

L'analyse marche mieux sur des accords tenus et clairs, avec peu de batterie. L'audio passe dans le plugin sans être modifié.
