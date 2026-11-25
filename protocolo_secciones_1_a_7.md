# Protocolo de Proyecto Final

**Materia:**      
**Periodo:** Agosto–Diciembre 2026

## 1. Datos generales

**Título tentativo del proyecto:** Generador procedural de jardines y entornos naturales tridimensionales  
**Nombre(s) de los integrantes:** [Escribir nombres completos]  
**Grupo:** [Escribir grupo]  
**Fecha de entrega:** [Escribir fecha]

## 2. Planteamiento de la idea

Se pretende desarrollar una aplicación tridimensional interactiva capaz de generar automáticamente un jardín o entorno natural delimitado a partir de una semilla numérica. Cada semilla producirá de manera reproducible la misma configuración de terreno, biomas, vegetación, rocas, camino y cuerpo de agua, mientras que una semilla diferente originará un escenario distinto. La finalidad del proyecto es demostrar, de forma visual y explorable, cómo diversos conceptos y algoritmos de Graficación por Computadora pueden integrarse para construir un espacio virtual coherente sin diseñar manualmente cada uno de sus elementos. La aplicación estará dirigida principalmente a estudiantes y docentes, quienes podrán recorrer el entorno en primera persona y consultar vistas de depuración que expliquen los datos y reglas utilizados durante su generación.

## 3. Justificación

La construcción manual de un entorno tridimensional con relieve, vegetación y elementos decorativos requiere colocar, orientar y ajustar una gran cantidad de objetos. Además de consumir tiempo, este proceso produce una única composición estática. La generación procedural atiende esta limitación mediante algoritmos capaces de crear distintas configuraciones a partir de parámetros y reglas, conservando coherencia visual y permitiendo reproducir un resultado mediante una semilla. Esta técnica tiene aplicaciones en videojuegos, simuladores, visualización, diseño de escenarios y producción de contenido digital.

Desde la perspectiva de la Graficación por Computadora, el proyecto permitirá aplicar los contenidos de la asignatura dentro de un sistema integrado y observable. El relieve se representará mediante una malla poligonal modificada con ruido procedural; los elementos del escenario se colocarán empleando transformaciones de traslación, rotación y escalamiento; y la cámara utilizará proyección en perspectiva para permitir la exploración. También se trabajará con normales, modelos de color, iluminación, sombreado, sombras y transparencia. Una ruta generada mediante una curva paramétrica permitirá aplicar splines, mientras que una familia sencilla de árboles utilizará ramificación recursiva para representar geometría fractal. El movimiento del agua y de la vegetación incorporará animaciones dependientes del tiempo.

El proyecto resulta relevante porque no se limitará a construir una escena visualmente agradable dentro de Unity. Los algoritmos centrales de generación del terreno, clasificación de biomas, distribución de objetos y construcción de la ruta serán desarrollados por el equipo. Asimismo, un modo de visualización permitirá observar mapas de altura, humedad, pendiente y biomas, además de estadísticas sobre los objetos generados. Esto facilitará comprobar y explicar el funcionamiento del sistema. Su alcance se mantendrá en un mapa delimitado y sin mecánicas complejas de videojuego, por lo que será posible dedicar tiempo a la estabilidad, el rendimiento, la presentación visual y la documentación académica.

## 4. Objetivos

### Objetivo general

Desarrollar una aplicación tridimensional interactiva que genere de manera procedural y reproducible un entorno natural delimitado, mediante la aplicación de mallas poligonales, transformaciones geométricas, iluminación, curvas paramétricas, distribución espacial, geometría fractal y animación.

### Objetivos específicos

1. Generar una malla de terreno mediante la combinación de ruido procedural en varias octavas y parámetros controlados por una semilla numérica.
2. Clasificar el terreno en tres biomas utilizando valores de altura, humedad y pendiente, con transiciones visuales graduales entre las diferentes regiones.
3. Distribuir vegetación y rocas mediante mapas de densidad, distancias mínimas y reglas que eviten ubicaciones incompatibles o intersecciones excesivas.
4. Construir una ruta principal mediante una curva paramétrica e implementar una familia de árboles con ramificación recursiva como demostración de geometría fractal.
5. Incorporar exploración en primera persona, iluminación, sombreado, animación ambiental y un modo de depuración que permita visualizar el proceso de generación.

## 5. Descripción técnica del proyecto

El proyecto será una aplicación 3D interactiva para computadora, presentada como un recorrido virtual con apariencia de una pequeña experiencia de videojuego. El escenario, de aproximadamente 256 × 256 metros, permanecerá delimitado para conservar un alcance viable. El usuario podrá desplazarse con el teclado y orientar una cámara en primera persona mediante el ratón. También se implementarán colisiones básicas con el terreno y límites del mapa. No se incluirán combate, misiones, inventario ni personajes autónomos.

Una semilla numérica inicializará la generación del escenario. El proceso será determinista: la misma semilla y los mismos parámetros deberán producir el mismo resultado. El terreno se construirá como una malla de vértices y triángulos, cuyas elevaciones se calcularán combinando varias octavas de ruido procedural con distintas frecuencias y amplitudes. También se generarán las coordenadas de textura y las normales necesarias para representar e iluminar la superficie. Los parámetros se restringirán para producir colinas, depresiones y áreas transitables propias de un jardín, parque o bosque pequeño.

Sobre la superficie se calcularán valores de altura, humedad y pendiente. Su combinación permitirá clasificar el mapa en tres biomas: pradera, bosque y zona rocosa-húmeda. Cada bioma tendrá reglas particulares de color, densidad y objetos permitidos. La vegetación incluirá árboles, arbustos, flores o hierbas, además de rocas low-poly. Su distribución utilizará máscaras de densidad y separación mínima. Antes de colocar un elemento se comprobarán su bioma, inclinación, cercanía al agua y al camino, y distancia respecto de otros objetos. La mayoría de los árboles reutilizará modelos optimizados; una familia limitada se generará mediante ramificación recursiva para demostrar geometría fractal.

El escenario tendrá una ruta principal definida mediante puntos de control y una curva spline. Las posiciones y tangentes obtenidas al evaluar la curva permitirán construir una franja ajustada al relieve y reservar a su alrededor un espacio sin árboles ni rocas. También se incorporará un estanque sencillo representado mediante una superficie transparente con color y movimiento de textura o normales, sin simulación de fluidos. La presentación gráfica empleará cámara en perspectiva, iluminación direccional y ambiental, sombras, niebla, materiales y una paleta low-poly coherente. La vegetación y el agua tendrán movimientos periódicos sencillos dependientes del tiempo.

La interfaz mostrará la semilla y estadísticas de los objetos generados. Además, el usuario podrá alternar la vista normal con mapas de altura, humedad, pendiente, biomas y ruta. Este modo de depuración permitirá relacionar el resultado tridimensional con los algoritmos utilizados. Para mantener un rendimiento estable se compartirán mallas y materiales, se utilizará renderización por instancias y se incorporarán niveles de detalle básicos únicamente cuando el perfilado demuestre que son necesarios.

## 6. Herramientas y tecnologías

- **Unity 6 en una versión LTS estable:** administración de escenas, renderizado, cámara, entrada del usuario, colisiones y herramientas de perfilado.
- **Universal Render Pipeline (URP):** materiales, iluminación, sombras, niebla y efectos visuales en tiempo real.
- **C#:** implementación de los algoritmos de generación procedural, reglas espaciales, interacción, interfaz y pruebas.
- **Shader Graph o HLSL básico:** creación del movimiento de la vegetación y del material estilizado del agua.
- **Blender:** creación o adaptación de modelos tridimensionales low-poly y posibles niveles de detalle.
- **Git y GitHub:** control de versiones, respaldo y colaboración entre los integrantes del equipo.

## 7. Relación con los temas de la asignatura

| Tema de Graficación por Computadora | Aplicación dentro del proyecto |
|---|---|
| Geometría afín y espacio euclídeo | Representación matemática de posiciones, direcciones, distancias y superficies del escenario. |
| Transformaciones geométricas | Traslación, rotación y escalamiento de árboles, plantas, rocas y demás objetos. |
| Representaciones matriciales y coordenadas homogéneas | Conversión de objetos entre coordenadas locales, globales y de cámara. |
| Mallas poligonales | Construcción del terreno, el camino y la geometría de los árboles recursivos mediante vértices y triángulos. |
| Proyección en perspectiva y volumen de vista | Representación del escenario desde la cámara del usuario y descarte de objetos fuera del campo visible. |
| Detección de superficies visibles | Uso de profundidad y eliminación de partes ocultas durante el renderizado. |
| Modelos de color | Definición de paletas y variaciones visuales para diferenciar los biomas. |
| Iluminación y sombreado | Aplicación de luz ambiental, difusa y especular, normales, materiales y sombras. |
| Transparencia | Representación visual de la superficie del agua. |
| Curvas paramétricas y splines | Definición de la trayectoria y construcción de la ruta principal. |
| Geometría fractal | Generación recursiva de troncos y ramas en una familia sencilla de árboles. |
| Animación, movimiento y tiempo | Oscilación periódica de la vegetación y movimiento visual del agua. |
| Renderizado | Integración final de geometría, cámara, materiales, iluminación, profundidad y efectos ambientales. |
