# Celeste Choir – plugin VST3 para FL Studio (Windows)

Sintetizador de coro (16 voces) con motor de formantes propio e interfaz celeste.

## Qué hace
- **Vocal A-E-I-O-U** con mezcla continua entre vocales (botones o slider).
- **Voz**: Soprano / Alto / Tenor / Bajo (cambia las formantes).
- **Formante**: desplaza el timbre (más chico / más grande).
- **Ensemble**: desafina varios cantantes por nota y los abre en estéreo (efecto "coro").
- **Aire**: ruido de respiración.
- **Vibrato** (profundidad y velocidad, entra con retardo como un cantante real).
- **ADSR** (ataque, caída, sostenido, liberación) + pedal de sustain (CC64).
- **Reverb** (tamaño y mezcla) y **Volumen**.

## Cómo compilar (Windows)
1. Instala **Visual Studio 2022 Community** con la carga de trabajo "Desarrollo para el escritorio con C++".
2. Instala **CMake** (https://cmake.org/download/) y **Git**.
3. Abre "x64 Native Tools Command Prompt for VS 2022" **como administrador** (para que copie el plugin a la carpeta VST3) en esta carpeta y ejecuta:

```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

La primera vez descarga JUCE 7.0.12 automáticamente (necesita internet).

4. El archivo queda en `build\CelesteChoir_artefacts\Release\VST3\Celeste Choir.vst3`
   y se copia solo a `C:\Program Files\Common Files\VST3`. Si no se copió, cópialo ahí a mano.

## En FL Studio
Options → Manage plugins → **Find installed plugins** (o "Verify plugins"). Aparece como **Celeste Choir** en la lista de generadores (Add → Celeste Choir). Toca con teclado MIDI o piano roll.

## Ideas para seguir
- Más vocales/palabras (sílabas), modo "legato", portamento.
- Un visualizador del espectro de formantes en la interfaz.
- Cambiar los colores en `PluginEditor.cpp` (busca `0xff7fd6ff`, el celeste).
