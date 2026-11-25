# Protocolo de Proyecto Final

**Materia:** Graficación por Computadora  
**Periodo:** Agosto–Diciembre 2026

## 1. Datos generales

**Título del proyecto:** Generador procedural de jardines y entornos naturales 3D  
**Integrantes:** Portocarrero Rodriguez Habid
**Grupo:** 6CV3
**Fecha de entrega:** 7.09.2026

## 2. Planteamiento de la idea

El proyecto consiste en desarrollar una aplicación tridimensional interactiva capaz de generar, a partir de una semilla numérica, un jardín o entorno natural delimitado y explorable. Cada semilla producirá de manera reproducible la misma forma del terreno, distribución de biomas, vegetación, rocas, camino y cuerpo de agua. La aplicación estará dirigida a estudiantes y docentes interesados en observar la aplicación práctica de algoritmos de graficación por computadora y generación procedural. Además de recorrer el escenario en primera persona, el usuario podrá consultar visualizaciones de los datos intermedios utilizados para construirlo, como los mapas de altura, humedad, pendiente y biomas.

## 3. Justificación

La creación manual de un entorno 3D con vegetación abundante exige colocar y ajustar una gran cantidad de elementos. La generación procedural permite producir variaciones coherentes de un escenario mediante algoritmos, parámetros y reglas reproducibles, disminuyendo el trabajo repetitivo y haciendo visible la relación entre matemáticas, programación y representación gráfica. Por ello, el proyecto no se limitará a presentar una escena decorada en un motor de videojuegos: el terreno, la clasificación de biomas, la distribución de objetos y la ruta principal se obtendrán mediante algoritmos implementados por el equipo.

La propuesta integra de manera natural contenidos de las tres unidades de la asignatura. Las transformaciones afines y los sistemas de coordenadas se aplicarán al posicionar, orientar y escalar objetos sobre una superficie; las mallas poligonales, normales, proyección en perspectiva, iluminación, sombreado y transparencia formarán parte del renderizado; y las curvas paramétricas, la geometría fractal y la animación dependiente del tiempo aparecerán en el camino, una familia de árbol recursivo y el movimiento del ambiente. Un modo de depuración permitirá relacionar el resultado visual con los campos numéricos y decisiones algorítmicas que lo originaron.

El alcance se mantiene deliberadamente acotado a un mapa aproximado de 256 × 256 metros, tres biomas y exploración sin combate, misiones ni sistemas de juego complejos. Esta delimitación hace viable obtener una demostración visualmente atractiva dentro del semestre y reservar tiempo para pruebas, optimización y documentación.

## 4. Objetivos

### Objetivo general

Desarrollar una aplicación 3D interactiva que genere de forma procedural y reproducible un entorno natural delimitado, aplicando algoritmos de mallas, transformaciones, iluminación, curvas, distribución espacial, geometría fractal y animación estudiados en Graficación por Computadora.

### Objetivos específicos

1. Generar una malla de terreno mediante la combinación de ruido procedural en varias octavas y parámetros derivados de una semilla.
2. Clasificar el terreno en tres biomas utilizando campos de altura, humedad y pendiente, con transiciones visuales no rectangulares.
3. Distribuir vegetación y rocas mediante máscaras de densidad, distancias mínimas y reglas que eviten posiciones incompatibles o intersecciones excesivas.
4. Construir una ruta principal a partir de una curva paramétrica y generar una demostración acotada de ramificación fractal para una familia de árbol.
5. Implementar exploración en primera persona y un modo de depuración que permita visualizar los datos de generación, además de evaluar el rendimiento de la escena.

## 5. Descripción técnica del proyecto

La aplicación será una experiencia 3D de escritorio en tiempo real. Al iniciar o regenerar el escenario, una semilla maestra alimentará un generador pseudoaleatorio determinista. De ella se derivarán subsemillas independientes para el terreno, humedad, biomas, ruta y decoración, de modo que cambiar un subsistema no altere de forma accidental todos los demás y que una misma configuración pueda repetirse.

El mundo abarcará aproximadamente 256 × 256 metros. Su superficie se representará mediante una malla poligonal cuya altura se calculará con una suma de varias octavas de ruido, semejante a un movimiento browniano fraccional. Los vértices, triángulos, coordenadas de textura y normales permitirán representar y sombrear el relieve. La amplitud, frecuencia, número de octavas y altura máxima se mantendrán dentro de intervalos predeterminados para producir colinas transitables en lugar de montañas extremas.

Sobre el terreno se calcularán campos continuos de altura, humedad y pendiente. La combinación ponderada de estos valores determinará tres biomas: pradera, bosque y zona rocosa-húmeda. Las fronteras se suavizarán mediante pesos de mezcla y variaciones de color y densidad. No se simulará clima físico ni se utilizará un mapa de temperatura, pues su aporte sería limitado en un área de esta escala.

La vegetación incluirá árboles, arbustos, flores o hierbas y rocas low-poly. La ubicación de los elementos utilizará muestreo con separación mínima y máscaras de densidad, no posiciones aleatorias sin restricciones. Cada candidato se validará según bioma, altura del agua, inclinación del terreno, cercanía al camino y distancia respecto de otros objetos. Los objetos grandes se colocarán antes que los pequeños y una rejilla espacial reducirá el costo de buscar vecinos. Se reutilizarán mallas y materiales para favorecer la renderización por instancias. Adicionalmente, una familia limitada de árboles utilizará reglas recursivas de tronco, ramas y follaje como demostración de geometría fractal; el resto empleará modelos low-poly reutilizables para mantener el rendimiento.

El escenario tendrá una ruta principal entre la entrada y un punto de interés. Su trayectoria se definirá mediante puntos de control y una curva spline; las muestras de la curva proporcionarán posición y tangente para construir una franja de ancho constante ajustada al relieve. La misma trayectoria generará una zona de exclusión que impedirá colocar árboles y rocas sobre el camino.

Se incluirá un estanque sencillo en una región baja apropiada. El agua se representará con una superficie transparente y un material estilizado con movimiento de normales o coordenadas de textura. No se realizará simulación de fluidos, erosión o ríos dinámicos. La escena usará cámara en perspectiva, iluminación direccional y ambiental, sombras, niebla global y materiales URP. El movimiento del viento en la vegetación y las ondas del agua dependerán del tiempo mediante funciones periódicas de baja complejidad.

El usuario recorrerá el mundo con teclado y ratón, con colisiones básicas contra el terreno y límites del mapa. Un panel mostrará la semilla y estadísticas como cantidades de árboles, flores y rocas. El modo académico permitirá alternar entre vista normal y representaciones de altura, humedad, pendiente, biomas y ruta, con el fin de demostrar que la composición visual procede de datos y reglas verificables.

El objetivo de rendimiento será mantener una interacción fluida en los equipos disponibles para la presentación. Se emplearán materiales compartidos, GPU instancing, culling proporcionado por el motor y niveles de detalle básicos para objetos grandes si el perfilado demuestra su necesidad. La cuadrícula lógica de generación facilitará las consultas espaciales, pero no se implementará un mundo infinito ni streaming de terreno.

## 6. Herramientas y tecnologías

- **Una versión LTS estable de Unity 6:** motor de renderizado, escenas, cámara, entrada, colisiones y perfilado.
- **Universal Render Pipeline (URP):** iluminación, materiales, sombras, niebla y shaders compatibles con distintas capacidades de hardware.
- **C#:** implementación de los algoritmos de generación, reglas espaciales, interfaz y pruebas.
- **Shader Graph o HLSL básico:** movimiento senoidal de vegetación y material estilizado de agua.
- **Blender:** elaboración o ajuste de modelos low-poly sencillos y niveles de detalle.
- **Git:** control de versiones y colaboración del equipo.

## 7. Relación con los temas de la asignatura

| Concepto del curso | Aplicación en el proyecto |
|---|---|
| Geometría afín, transformaciones y coordenadas homogéneas | Posición, rotación y escalamiento de instancias; conversión entre coordenadas de malla, mundo y cámara. |
| Mallas poligonales y representación de sólidos | Construcción del terreno, camino y árboles recursivos mediante vértices y triángulos. |
| Proyección en perspectiva y volumen de vista | Cámara de exploración y descarte de elementos fuera de la vista. |
| Superficies visibles y partes ocultas | Pruebas de profundidad y culling durante el renderizado. |
| Modelos de color | Paletas diferenciadas y mezclas según bioma. |
| Iluminación y sombreado | Luz ambiental, componente difusa y especular, normales y sombras. |
| Transparencia | Representación del agua. |
| Curvas paramétricas y splines | Trayectoria y geometría de la ruta principal. |
| Geometría fractal | Ramificación recursiva de una familia de árbol. |
| Movimiento, tiempo y animación | Oscilación periódica de vegetación y agua. |

## 8. Cronograma de actividades

| Actividad | Semanas |
|---|---|
| Investigación, definición de requisitos, repositorio y prototipos matemáticos | 1–2 |
| Semilla determinista, malla de terreno, normales y parámetros | 3 |
| Campos ambientales, clasificación y visualización de biomas | 4 |
| Muestreo espacial, reglas de exclusión y colocación de vegetación/rocas | 5–6 |
| Exploración, colisiones, límites y regeneración del mundo | 7 |
| Spline y malla de la ruta; estanque sencillo | 8 |
| Árbol recursivo, iluminación, niebla, agua y viento | 9 |
| Modo de depuración, pruebas de reproducibilidad y perfilado | 10 |
| Corrección de errores, optimización, documentación y presentación final | 11 |

Cada etapa deberá producir una versión ejecutable. Las posibles extensiones sólo se considerarán cuando las funciones comprometidas estén terminadas, verificadas y documentadas.

## 9. Resultados esperados

Se espera obtener una aplicación ejecutable en la que diferentes semillas generen jardines 3D distintos y una misma semilla reproduzca el mismo escenario. El resultado contendrá un terreno transitable, tres biomas con límites naturales, vegetación y rocas distribuidas mediante reglas, una ruta curva, un estanque, iluminación y animación ambiental. El usuario podrá explorar el mapa y activar vistas de depuración para observar los campos y estadísticas que explican su construcción.

Además del ejecutable, se entregarán el código fuente comentado, parámetros de generación, evidencia de pruebas de reproducibilidad y rendimiento, modelos o recursos propios y la documentación técnica. Como extensiones no comprometidas podrán considerarse un cuarto bioma, ciclo día/noche, partículas, modo fotográfico o mayor variedad procedural, siempre que la versión principal se encuentre completa.

## 10. Referencias bibliográficas

Bridson, R. (2007). Fast Poisson disk sampling in arbitrary dimensions. *ACM SIGGRAPH 2007 Sketches*, 22. https://doi.org/10.1145/1278780.1278807

Perlin, K. (2002). Improving noise. *ACM Transactions on Graphics, 21*(3), 681–682. https://doi.org/10.1145/566654.566636

Prusinkiewicz, P., & Lindenmayer, A. (1990). *The algorithmic beauty of plants*. Springer-Verlag. https://doi.org/10.1007/978-1-4613-8476-2

Unity Technologies. (2025). *Universal Render Pipeline*. Unity Manual. https://docs.unity3d.com/6000.0/Documentation/Manual/universal-render-pipeline.html

Unity Technologies. (2025). *GPU instancing*. Unity Manual. https://docs.unity3d.com/6000.0/Documentation/Manual/GPUInstancing-landing.html

## Delimitación del compromiso

La propuesta **sí compromete** terreno procedural, semillas reproducibles, tres biomas, distribución espacial controlada, un camino por spline, agua sencilla, exploración, una demostración fractal, iluminación, animación ambiental, modo de depuración y optimización básica.

La propuesta **no compromete** mundo infinito, streaming, generación planetaria, clima simulado, ecosistemas, multijugador, NPC, combate, inventario, crafting, misiones, simulación de fluidos, ray tracing, fotorealismo ni calidad comercial. El ciclo día/noche, las partículas y los biomas adicionales se consideran únicamente posibles extensiones.
