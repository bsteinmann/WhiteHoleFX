# WhiteHoleFX – Tutorial

Este tutorial te lleva en unos 30 minutos desde el primer inicio hasta tu propio efecto en tus LED. El mismo tutorial existe en
[alemán](de.md), [inglés](en.md) y [francés](fr.md). En el programa lo encuentras en **Ayuda → Tutorial**, en el idioma que
elegiste en **Ayuda → Sprache / Language**.

## 1. Qué hace WhiteHoleFX

WhiteHoleFX controla matrices LED, strips, cubos y cualquier disposición de píxeles. Construyes un efecto con **nodos** (bloques
como degradado, plasma, rotar, espejo) que conectas con cables. El efecto se calcula en la tarjeta gráfica, se muestrea para cada
LED en su posición del espacio y se envía a tus controladores por **Art-Net**, **sACN** o **DDP**.

Necesitas tres términos:

- **Fixture:** una pieza de hardware con LED, por ejemplo una matriz, un strip o una cuadrícula 3D (cubo).
- **Patch:** adónde envía un fixture sus datos (dirección del controlador, universo, canal, orden de colores).
- **Look:** un efecto terminado y guardado que recuperas con un clic en el modo live.

## 2. Primer inicio

1. Inicia `whitehole_studio`.
2. Abre un ejemplo: **Archivo → Ejemplos → Primeros pasos → wand**. El ejemplo consta de una matriz de 16×8 LED y un strip.
3. Pulsa la **barra espaciadora** (iniciar). La vista previa del centro muestra el efecto en marcha.

**Importante:** tras cada inicio la salida de red está **apagada**. El proyecto funciona y se ve en la vista previa, pero ningún
paquete sale de tu ordenador. Solo **Reproducción → Enviar salida** envía datos de verdad a los controladores; el botón se
ilumina entonces en rojo. Así nada puede encenderse por error en una instalación ajena.

## 3. Montar el escenario (Stage Config)

Si empiezas de cero, empieza por el hardware. Abre **Stage Config → Disposición y patch …** (F4). La ventana aparte tiene estas
páginas:

- **Fixtures:** la lista de todos los fixtures. **Añadir fixture …** abre un diálogo donde indicas el tipo (matriz, strip,
  cuadrícula 3D), el tamaño en x, y y z, la separación, la posición en el espacio y el cableado: la esquina de inicio, si el
  cable va en serpentina y qué eje cuenta más rápido. La pequeña vista 3D de arriba muestra al instante cómo queda con cada
  valor. El primer LED del cable es **blanco**, así reconoces la esquina de inicio.
- **Destinos (controladores):** las direcciones de tus controladores. Una dirección vacía significa multicast.
- **Modo de prueba:** envía imágenes de prueba (color, barra de barrido, orden de colores, secuencia) directamente a los LED para
  comprobar cableado y patch sin que corra ningún efecto. **Atención:** el blanco se enciende a pleno brillo; revisa antes las
  fuentes de alimentación.
- **Vista:** la instalación en el espacio, giratoria, con un color propio por fixture y un eje de coordenadas (x rojo, y verde, z azul).

Un cubo se crea como **cuadrícula 3D**, por ejemplo de 8 × 8 × 8 LED. Para strips enrollados en una columna usa una **nube de
puntos**: **Stage Config → Importar nube de puntos …** lee las posiciones de los LED desde un archivo CSV o JSON.

Consejo: abre **Archivo → Ejemplos → 3D → ausrichtung**. Cada LED muestra su posición como color (rojo = x, verde = y, azul = z).
Si la imagen no coincide con tu montaje, cambia la esquina de inicio y el orden en el fixture.

## 4. Construir un efecto (editor de nodos)

Abajo en la ventana está el panel **Nodos**: la **paleta** a la izquierda, el **editor** a la derecha.

- **Añadir un nodo:** arrástralo de la paleta al lienzo, haz doble clic, clic derecho en el lienzo vacío, o pulsa
  **Ctrl+Espacio** y escribe («rain», «mix», «blend add»).
- **Conectar:** arrastra de una salida (rellena, a la derecha) a una entrada (anillo, a la izquierda). El color y la forma de un
  puerto indican su tipo y solo se conectan puertos compatibles. Una entrada ocupada se sustituye.
- **Elegir la salida:** clic derecho en un nodo → **Establecer como salida (output)**. Este nodo decide lo que aparece en los LED
  (contorno naranja).
- **Eliminar:** haz clic en un nodo y pulsa **Supr**. Si está dentro de una cadena (A → X → B), el editor conecta A **directamente
  con B**, de modo que la cadena no se rompe. **Ctrl+Z** lo deshace todo.
- **Si falta una entrada,** los LED quedan **apagados** (la línea de estado indica «FALTA UNA ENTRADA») en lugar de congelar la
  última imagen. En cuanto el grafo vuelve a estar completo, el efecto continúa.

**Favoritos:** las categorías de la paleta están plegadas al inicio. Con clic derecho en un tipo de nodo lo añades al grupo
**Favoritos**, que queda arriba y se recuerda. Al buscar, los resultados aparecen desplegados.

Un ejemplo pequeño: arrastra **Linear Gradient** y **Rotate** al lienzo, conecta `out` del degradado con `source` de Rotate y
establece Rotate como salida. Después añade **Time** y conecta `seconds` con la entrada `angle` de Rotate: el degradado gira.

## 5. Ajustar parámetros

En el panel **Parámetros** (derecha) cada nodo tiene sus controles: números, listas de opciones, interruptores sí/no y colores.
Al hacer clic en un nodo del editor se muestran sus parámetros. Los controles conectados a algo (por ejemplo un LFO) aparecen en
gris. El símbolo ↺ restablece el valor predeterminado. En **Edición → Ajustes → Valores predeterminados de nodos** decides con qué
valores empiezan los nodos nuevos.

## 6. 3D

Todos los generadores conocen la profundidad (z). En 2D (una matriz) no cambia nada; en un cubo el efecto llena el espacio. Prueba
los cinco ejemplos 3D en **Archivo → Ejemplos → 3D**:

- **ausrichtung:** comprueba ejes y cableado (Position Test).
- **plasma-wuerfel:** un plasma que recorre la profundidad, con gradiente de color y rotación.
- **scheibe:** una rebanada atraviesa el cubo.
- **regentropfen:** gotas como capas esféricas en el espacio.
- **saeule:** un strip enrollado en hélice alrededor de una columna.

La barra **Vista** cambia entre **2D (editar)** y las vistas 3D (libre, frontal, lateral, superior). En 3D se gira arrastrando, el
clic derecho desplaza y la rueda hace zoom. En **Vista → Presentación 3D** ocultas los LED apagados (predeterminado, para ver a
través del cubo) y activas el brillo aditivo.

## 7. Looks y modo live

Un efecto que te gusta se guarda con **Ctrl+L** como **look** (aparece en el panel **Looks** y en el archivo del proyecto). **F9**
inicia el **modo live**: el editor desaparece y cada look tiene un botón grande. Un clic (o la tecla 1 a 9) hace el fundido en el
tiempo ajustado. Con «Con GO» solo precargas un look e inicias el fundido con la **barra espaciadora**. **Esc** termina el modo
live. **Master** regula el brillo global y **Ctrl+B** es el blackout.

## 8. Audio y DMX como entradas

En **Edición → Ajustes** eliges la tarjeta de red, el dispositivo de audio y la entrada DMX. Los nodos de audio (nivel, banda de
frecuencia, beat, espectro, osciloscopio) reaccionan entonces a la música; los nodos DMX leen valores de una mesa de luces. Hay
ejemplos en **Archivo → Ejemplos → Audio**.

## 9. Cambiar el idioma

**Ayuda → Sprache / Language** ofrece Deutsch, English, Français y Español. La elección se guarda; si quieres, el programa se
reinicia al instante y vuelve a abrir tu proyecto. Los nombres de nodos y parámetros siguen en inglés para que las guías y los
proyectos de todos los idiomas encajen entre sí.

## 10. Cuando algo no funciona

- **No hay luz en los LED:** ¿está activado **Enviar salida**? ¿Es correcta la dirección del controlador en Destinos? ¿Ayuda el
  modo de prueba?
- **Colores incorrectos:** cambia el orden de colores en el patch (RGB, GRB …) o ejecuta «Orden de colores» en el modo de prueba.
- **Imagen del revés o en espejo:** cambia la esquina de inicio y las opciones `flip` del fixture.
- **Tirones:** baja la velocidad de fotogramas o reduce la ventana; en cambios en directo, precarga con «Con GO».
- **Cualquier otra cosa:** **Ayuda → Informar de un error o sugerir una idea …** abre GitHub.

Para saber más, lee los documentos detallados de la carpeta `docs/`: interfaz, archivo de proyecto, nodos de campo, mapping,
salida, audio y grafo de valores.
