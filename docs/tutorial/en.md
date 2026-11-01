# WhiteHoleFX – Tutorial

This tutorial takes you from the first start to your own effect on your LEDs in about 30 minutes. The same tutorial exists in
[German](de.md), [French](fr.md) and [Spanish](es.md). In the program you find it under **Help → Tutorial**, in the language you
chose under **Help → Sprache / Language**.

## 1. What WhiteHoleFX does

WhiteHoleFX controls LED matrices, strips, cubes and any arrangement of pixels. You build an effect from **nodes** (building
blocks such as gradient, plasma, rotate, mirror) that you connect with cables. The effect is computed on the graphics card,
sampled for every LED at its position in space and sent to your controllers over **Art-Net**, **sACN** or **DDP**.

You need three terms:

- **Fixture:** a piece of hardware with LEDs, for example a matrix, a strip or a 3D grid (cube).
- **Patch:** where a fixture sends its data (controller address, universe, channel, color order).
- **Look:** a saved, finished effect that you recall with one click in live mode.

## 2. First start

1. Start `whitehole_studio`.
2. Open an example: **File → Examples → Getting started → wand**. The example consists of a 16×8 LED matrix and a strip.
3. Press the **Space bar** (start). The preview in the middle shows the running effect.

**Important:** After every start the network output is **off**. The project runs and shows in the preview, but no packet leaves
your computer. Only **Playback → Send output** really sends data to the controllers; the button then lights up red. This way
nothing can accidentally light up on someone else's rig.

## 3. Setting up the stage (Stage Config)

If you are new, start with the hardware. Open **Stage Config → Layout and Patch …** (F4). The separate window has these pages:

- **Fixtures:** the list of all fixtures. **Add fixture …** opens a dialog where you enter the type (matrix, strip, 3D grid), the
  size in x, y and z, the spacing, the position in space and the wiring: the start corner, whether the cable runs in a
  serpentine, and which axis counts fastest. The small 3D view on top shows how it looks with every value you change. The first
  LED on the cable is **white**, so you can recognize the start corner.
- **Destinations (controllers):** the addresses of your controllers. An empty address means multicast.
- **Test mode:** sends test patterns (color, sweep bar, color order, chase) straight to the LEDs to check wiring and patch
  without an effect running. **Caution:** white lights up at full brightness; check the power supplies first.
- **View:** the rig in space, rotatable, with its own color per fixture and an axis cross (x red, y green, z blue).

You create a cube as a **3D grid**, for example 8 × 8 × 8 LEDs. For strips wound around a column use a **point cloud**:
**Stage Config → Import point cloud …** reads the LED positions from a CSV or JSON file.

Tip: open **File → Examples → 3D → ausrichtung**. Every LED shows its position as a color (red = x, green = y, blue = z). If the
picture does not match your build, change the start corner and the order in the fixture.

## 4. Building an effect (node editor)

At the bottom of the window is the **Nodes** dock: the **palette** on the left, the **editor** on the right.

- **Add a node:** drag it from the palette onto the canvas, double-click it, right-click the empty canvas, or press
  **Ctrl+Space** and type ("rain", "mix", "blend add").
- **Connect:** drag from an output (filled, on the right) to an input (ring, on the left). The color and shape of a port show its
  type, and only matching ports connect. An occupied input is replaced.
- **Choose the output:** right-click a node → **Set as output**. This node decides what appears on the LEDs (orange outline).
- **Delete:** click a node, press **Del**. If it sits in a chain (A → X → B), the editor connects A **directly to B**, so the chain
  stays intact. **Ctrl+Z** undoes everything.
- **If an input is missing,** the LEDs go **dark** (the status line says "INPUT MISSING") instead of freezing the last picture.
  As soon as the graph is complete again, the effect continues.

**Favorites:** the palette categories are collapsed at start. Right-click a node type to add it to the **Favorites** group; it
stays on top and is remembered. While searching, matches are expanded.

A small example: drag **Linear Gradient** and **Rotate** onto the canvas, connect the gradient's `out` to Rotate's `source`, set
Rotate as the output. Then add **Time** and connect `seconds` to Rotate's `angle` input: the gradient rotates.

## 5. Adjusting parameters

In the **Parameters** dock (right) every node has its controls: numbers, choice lists, yes/no switches and colors. Clicking a
node in the editor shows its parameters. Controls that are connected to something (for example an LFO) are grayed out. The ↺
symbol restores the default value. Under **Edit → Settings → Node defaults** you decide which values new nodes start with.

## 6. 3D

All generators know depth (z). In 2D (a matrix) this changes nothing; in a cube the effect fills the space. Try the five 3D
examples under **File → Examples → 3D**:

- **ausrichtung:** checks axes and wiring (Position Test).
- **plasma-wuerfel:** plasma running through the depth, with a color ramp and rotation.
- **scheibe:** a slice moves through the cube.
- **regentropfen:** drops as spherical shells in space.
- **saeule:** a strip wound in a helix around a column.

The **View** toolbar switches between **2D (edit)** and the 3D views (free, front, side, top). In 3D you rotate by dragging,
right-click pans, the mouse wheel zooms. Under **View → 3D display** you switch off LEDs to hidden (default, so you can see
through the cube) and additive glow on.

## 7. Looks and live mode

An effect you like is saved with **Ctrl+L** as a **look** (it appears in the **Looks** dock and in the project file). **F9**
starts **live mode**: the editor disappears and every look gets a big button. A click (or key 1 to 9) fades over in the set
time. With "With GO" you only preload a look and start the fade with the **Space bar**. **Esc** ends live mode. **Master**
controls the overall brightness, **Ctrl+B** is the blackout.

## 8. Audio and DMX as inputs

Under **Edit → Settings** you choose the network card, the audio device and the DMX input. Audio nodes (level, frequency band,
beat, spectrum, oscilloscope) then react to music; DMX nodes read values from a lighting desk. You find examples under
**File → Examples → Audio**.

## 9. Changing the language

**Help → Sprache / Language** offers Deutsch, English, Français and Español. The choice is saved; if you like, the program
restarts at once and reopens your project. Names of nodes and parameters stay English so that guides and projects in all
languages fit together.

## 10. When something does not work

- **No light on the LEDs:** is **Send output** on? Is the controller address under Destinations right? Does test mode help?
- **Wrong colors:** change the color order in the patch (RGB, GRB …), or run "Color order" in test mode.
- **Picture upside down or mirrored:** change the start corner and the `flip` options in the fixture.
- **Stuttering:** lower the frame rate or shrink the window; for live changes preload with "With GO".
- **Anything else:** **Help → Report a bug or suggest an idea …** opens GitHub.

For more, read the detailed documents in the `docs/` folder: user interface, project file, field nodes, mapping, output, audio
and value graph.
