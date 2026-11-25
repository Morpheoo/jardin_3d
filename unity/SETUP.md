# Montaje de la escena — Fase 0

Qué hacer cuando termine de bajar Unity.

## 1. Crear el proyecto

Unity Hub → *New Project* → plantilla **Universal 3D** (es URP) → Unity 6 LTS.

Copia la carpeta `Assets/Scripts/` de aquí dentro del `Assets/` del proyecto nuevo.

## 2. Material del terreno

`Assets` → clic derecho → *Create* → *Material*. Llámalo `M_Terrain`.
En el inspector, shader **Universal Render Pipeline/Lit**. Déjalo gris por ahora:
el color viene en la Fase 1.

## 3. Prefabs provisionales

No hace falta descargar nada todavía. Para ver algo hoy:

- *GameObject* → *3D Object* → *Cylinder*, escálalo a (0.3, 2, 0.3) → arrástralo a `Assets` para hacerlo prefab. Llámalo `P_ArbolA`.
- Otro cilindro más bajo y ancho → `P_ArbolB`.
- *3D Object* → *Sphere*, escala (0.8, 0.5, 0.8) → `P_Roca`.

Feos a propósito. Sirven para verificar que la colocación funciona.
Los assets de Kenney o Quaternius entran en la Fase 1.

> Importante: quítales el **Collider** a los props. Cientos de colliders no aportan
> nada en la Fase 0 y cuestan rendimiento. El jugador colisiona con el terreno.

## 4. Jerarquía de la escena

```
World                 (GameObject vacío en 0,0,0)
 ├── WorldBootstrap    (componente)
 └── PropSpawner       (componente, en el MISMO objeto)

Player                (GameObject vacío)
 ├── CharacterController
 ├── FirstPersonController
 └── Main Camera       (hija, en 0, 0.8, 0)

HUD                   (GameObject vacío)
 └── WorldHud
```

Conexiones en el inspector:

| Componente | Campo | Valor |
|---|---|---|
| `WorldBootstrap` | Terrain Material | `M_Terrain` |
| `WorldBootstrap` | Props | el `PropSpawner` del mismo objeto |
| `PropSpawner` | Layers → prefab (×3) | `P_ArbolA`, `P_ArbolB`, `P_Roca` |
| `FirstPersonController` | World | el objeto `World` |
| `FirstPersonController` | Camera Pivot | la `Main Camera` |
| `WorldHud` | World / Props | `World` y su `PropSpawner` |

En el `CharacterController` del Player: Height **1.8**, Radius **0.35**.

## 5. Probar

**Sin entrar en Play:** clic derecho en `WorldBootstrap` → **Regenerar**.
Debe aparecer el terreno con los objetos encima.

**Play:** WASD para andar, ratón para mirar, Shift para correr, Escape libera el cursor.

| Tecla | Acción |
|---|---|
| TAB | Cambiar vista (Normal → Altura → Humedad → Pendiente → Zonas → Mezcla) |
| R | Regenerar con la misma semilla |
| N | Semilla nueva |
| F1 | Ocultar el panel |

## 6. Verificar el determinismo

Crea un GameObject vacío, agrégale `DeterminismCheck`, clic derecho en el
componente → **Probar determinismo**. Debe salir en verde en la consola.

Luego **Probar que semillas distintas dan mundos distintos**.

> Esto se ejecuta **al terminar cada fase**, no solo cuando algo se vea raro.
> Es la Regla de control 2 del plan maestro.

---

## Qué esperar, y qué no

El terreno va a verse **gris y plano de luz**, y los objetos son cilindros.
Eso es correcto: la Fase 0 demuestra que los algoritmos funcionan, no que se vea bien.
Todo el aspecto visual llega en la Fase 1 y es donde el proyecto cambia de golpe.

Lo que sí debe cumplirse antes de congelar la Fase 0:

- [ ] Consola sin errores
- [ ] La misma semilla da el mismo mundo dos veces (verificado con `DeterminismCheck`)
- [ ] Se recorre el mapa completo sin atravesar el terreno ni salirse
- [ ] Las seis vistas del HUD funcionan
- [ ] Captura guardada

---

## Ajustes que probablemente querrás

Detectados al probar el núcleo fuera de Unity:

**Pocos árboles.** Con los valores por defecto salen 67 + 71 árboles y 285 rocas
en 128 × 128 m. Se ve escaso y las rocas dominan. Para densificar:
baja `spacing` de las capas de árbol (4.5 → 3.0) o baja su `densityThreshold`.
Para menos rocas, sube el `densityThreshold` de `Roca` (0.62 → 0.75).

**El terreno sube menos de lo que dice `maxHeight`.** La altura normalizada llega
a ~0.67, no a 1.0, por el `heightExponent`. Con `maxHeight = 14` el relieve real
sube ~9.4 m. Si lo quieres más dramático, sube `maxHeight` o baja `heightExponent`.

**Iterar rápido.** Baja `resolution` a 96 mientras ajustas parámetros y súbela a 192
para las capturas. Está verificado que el paisaje **no cambia** al cambiar la
resolución, así que puedes hacerlo sin miedo.
