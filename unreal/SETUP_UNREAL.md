# Montaje en Unreal Engine 5.7 — Fase 0

## 0. BLOQUEO: falta .NET Framework SDK

Antes de nada. Intenté compilar el módulo con UnrealBuildTool y falló:

```
Could not find NetFxSDK install dir; this will prevent SwarmInterface from installing.
Install a version of .NET Framework SDK at 4.6.0 or higher.
Result: Failed (RulesError)
```

Comprobado en tu máquina:

| Componente | Estado |
|---|---|
| Windows SDK `10.0.26100` | ✅ presente |
| MSVC `14.44.35207` | ✅ presente |
| `Windows Kits\NETFXSDK` | ❌ **no existe** |
| Targeting packs de .NET Framework | ❌ **no existen** |

Tu Visual Studio 2022 se instaló sin los componentes de .NET Framework. **No es un problema de Unreal ni del código** — te bloquearía igual al pulsar Compilar en el editor.

### Cómo arreglarlo

1. Abre **Visual Studio Installer** (búscalo en el menú Inicio)
2. En Visual Studio Community 2022 → **Modificar**
3. Pestaña **Cargas de trabajo** → marca **Desarrollo de juegos con C++**
4. Pestaña **Componentes individuales** → busca `.NET Framework` y marca:
   - **SDK de .NET Framework 4.8** (o 4.6.2, cualquiera ≥ 4.6)
   - **Paquete de compatibilidad para .NET Framework 4.8** *(targeting pack)*
5. **Modificar** y esperar

Si el instalador te ofrece **Unreal Engine installer** como componente opcional dentro de la carga de trabajo de juegos, márcalo también.

### Verificar que quedó

Debe existir la carpeta:

```
C:\Program Files (x86)\Windows Kits\NETFXSDK\
```

---

## 1. Compilar el módulo

Con lo anterior resuelto:

```bat
"C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" JardinEditor Win64 Development -Project="<ruta>\unreal\Jardin.uproject" -WaitMutex
```

O más cómodo: clic derecho en `Jardin.uproject` → **Generate Visual Studio project files**, abrir `Jardin.sln`, y compilar desde VS.

La primera compilación tarda varios minutos. Las siguientes, segundos.

> Si salen errores de compilación, pásamelos. La capa de Unreal está escrita pero **no ha podido compilarse todavía** por el bloqueo de arriba, así que es normal que haya que corregir detalles de API.

---

## 2. Estructura del proyecto

```
unreal/
├── Jardin.uproject
└── Source/
    ├── Jardin.Target.cs
    ├── JardinEditor.Target.cs
    └── Jardin/
        ├── Jardin.Build.cs
        ├── Jardin.cpp                 módulo
        ├── Core/                      ← EL NÚCLEO: C++ plano, cero Unreal
        │   ├── JardinCore.h/.cpp      hash, RNG, SeedBook, ruido, fBm
        │   ├── JardinWorld.h/.cpp     settings, WorldData, campos, zonas
        │   └── JardinScatter.h/.cpp   rejilla espacial, reglas, repartidor
        ├── JardinTypes.h/.cpp         puente USTRUCT ↔ núcleo
        └── JardinWorldActor.h/.cpp    malla, HISM, texturas de debug
```

**`Core/` no incluye una sola cabecera de Unreal.** Se compila y se prueba con `cl.exe` suelto — es lo que demuestra que el algoritmo es tuyo y no configuración del motor.

---

## 3. Las dos trampas de Unreal, ya resueltas en el código

**Centímetros.** UE trabaja en cm, el núcleo en metros. La conversión vive solo en `JardinUnits::MetersToUU = 100.0f`. Tu mapa de 128 m son 12 800 unidades.

**Z-up.** En UE la altura es **Z**, no Y. El mapeo es:

```
núcleo (X, Z, altura)  →  Unreal (X, Y, Z)
```

Como consecuencia, la mano del sistema de coordenadas cambia y **el sentido de giro de los triángulos se invierte** respecto a la versión de Unity. Puse el orden que creo correcto, más una casilla `bInvertTriangleWinding` en el inspector.

> **Síntoma de que hay que activarla:** el terreno se ve desde abajo pero desaparece visto desde arriba. Es un clic, no un bug.

---

## 4. Montar la escena

### Mallas provisionales

No descargues nada aún. En el editor:

- **Cube** escalado a (0.3, 0.3, 2) → guardar como Static Mesh `SM_ArbolA`
- Otro más bajo y ancho → `SM_ArbolB`
- **Sphere** escalada a (0.8, 0.8, 0.5) → `SM_Roca`

Feas a propósito. Sirven para verificar la colocación. Los assets buenos entran en la Fase 1.

### El actor

1. Arrastra **JardinWorldActor** desde el Content Browser (pestaña C++ Classes) a la escena
2. Colócalo en **(0, 0, 0)**
3. En el inspector:

| Campo | Valor |
|---|---|
| Settings → Seed | 827461 |
| Settings → World Size Meters | 128 |
| Settings → Resolution | 192 |
| Terrain Material | cualquier material básico |
| Layers → [0] Mesh | `SM_ArbolA` |
| Layers → [1] Mesh | `SM_ArbolB` |
| Layers → [2] Mesh | `SM_Roca` |
| Random Seed On Begin Play | ✅ (jardín nuevo cada vez que entras) |

4. Botón **Generate** en el inspector → el jardín aparece **sin entrar en Play**

Tocar cualquier parámetro regenera solo. Con ~7 ms de generación, ajustar el ruido es interactivo.

### Jugador

Usa el **Player Start** de la plantilla First Person. Colócalo por encima del terreno; la gravedad lo asienta.

---

## 5. Verificar antes de congelar la Fase 0

Botón **Check Determinism** en el inspector del actor. Busca en el Output Log:

```
[Determinismo] CORRECTO. Semilla 827461 -> 64C68E3D, estable en 3 ejecuciones.
```

Ese `64C68E3D` es el mismo checksum que dan la versión C# y el núcleo compilado suelto. **Si Unreal da ese número, toda la cadena está íntegra.**

Lista de cierre:

- [ ] Compila sin errores
- [ ] `Check Determinism` en verde
- [ ] El terreno se ve desde arriba (si no: `bInvertTriangleWinding`)
- [ ] Se recorre el mapa completo sin atravesar el suelo
- [ ] Los árboles están sobre el terreno, no flotando ni hundidos
- [ ] Captura guardada

---

## 6. El HUD de depuración (Blueprint)

El actor expone todo lo necesario. Crea un **Widget Blueprint** con un `Image` y llama:

- `Create Debug Texture(View)` → devuelve el `UTexture2D` del mapa elegido
- `Get Seed`, `Get Total Props`, `Get Prop Counts`, `Get Checksum`

Vistas disponibles: Normal, Altura, Humedad, Pendiente, Zonas, Fronteras de zona.

Agregar una vista nueva (densidad, o la máscara del camino de la Fase 4) es **un `case` más** en `CreateDebugTexture`. Esa es la costura 7.

---

## 7. Ajustes que vas a querer

Medidos en el núcleo, no adivinados:

**Pocos árboles.** Con los valores por defecto salen 67 + 71 árboles y 285 rocas en 128 × 128 m. Se ve escaso y las rocas dominan.

- Más árboles: baja `Spacing Meters` de las capas de árbol (4.5 → 3.0)
- Menos rocas: sube `Density Threshold` de `Roca` (0.62 → 0.75)

**El terreno sube menos de lo que dice `MaxHeightMeters`.** La altura normalizada llega a ~0.67, no a 1.0, por el `HeightExponent`. Con 14 m el relieve real sube ~9.4 m.

**Iterar rápido.** Baja `Resolution` a 96 mientras ajustas y súbela a 192 para las capturas. Está verificado que el paisaje **no cambia** con la resolución.

---

## 8. Qué esperar

Terreno gris, luz plana, cubos como árboles. **Es correcto.** La Fase 0 demuestra que los algoritmos funcionan, no que se vea bien.

El salto visual es la Fase 1 — y en Unreal tienes Lumen y el post-proceso por defecto a favor, así que ese salto va a ser aún más marcado que en Unity.
