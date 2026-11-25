# Protocolo de Proyecto Final

**Materia:** Graficación por Computadora  
**Periodo:** Agosto–Diciembre 2026

## 1. Datos generales

**Título tentativo del proyecto:** Generador procedural de un entorno natural 3D  
**Nombre(s) de los integrantes:** [Escribir nombres completos]  
**Grupo:** [Escribir grupo]  
**Fecha de entrega:** [Escribir fecha]

## 2. Planteamiento de la idea

El proyecto consistirá en una aplicación tridimensional que genere automáticamente un pequeño entorno natural a partir de una semilla numérica. El escenario estará formado por un terreno con relieve, dos tipos de zona —pradera y bosque—, árboles y rocas colocados mediante reglas sencillas. La misma semilla producirá siempre el mismo escenario. El usuario podrá recorrer el mapa en primera persona y activar una vista de depuración para observar cómo los datos de altura y tipo de zona determinan el resultado. La aplicación estará dirigida a estudiantes y docentes, y tendrá como finalidad demostrar de manera clara y visual la aplicación de conceptos fundamentales de Graficación por Computadora.

## 3. Justificación

La creación manual de un escenario tridimensional requiere modelar el terreno y colocar individualmente cada elemento. La generación procedural permite automatizar parte de este proceso mediante algoritmos y parámetros, haciendo posible obtener distintas variaciones de un entorno sin construir cada una desde cero. Además, el uso de una semilla numérica permite reproducir un escenario específico, lo que facilita realizar pruebas y comparar resultados.

Desde la perspectiva de la Graficación por Computadora, el proyecto permitirá integrar conceptos fundamentales de la asignatura en una aplicación concreta. El terreno se representará mediante una malla de vértices y triángulos, cuyas alturas se modificarán utilizando ruido procedural. Los árboles y rocas se ubicarán por medio de transformaciones de traslación, rotación y escalamiento. Para visualizar el escenario se empleará una cámara con proyección en perspectiva, junto con iluminación, materiales, normales y sombreado.

El valor académico del proyecto estará en los algoritmos desarrollados por el equipo. La generación del relieve, el manejo de la semilla, la clasificación de las zonas y la colocación de objetos no dependerán de un escenario previamente construido. Una vista de depuración mostrará el mapa de alturas y la clasificación del terreno, con lo cual será posible explicar la relación entre los datos generados y el resultado visual.

El alcance se limitará a un mapa pequeño, dos tipos de zona y una cantidad reducida de elementos gráficos. No se desarrollarán sistemas de combate, misiones, clima, agua, caminos, árboles fractales ni simulaciones complejas. Esta delimitación permitirá concentrar el trabajo en completar, probar y documentar correctamente las funciones principales dentro del tiempo disponible.

## 4. Objetivos

### Objetivo general

Desarrollar una aplicación 3D interactiva que genere de forma procedural y reproducible un entorno natural pequeño, aplicando conceptos fundamentales de mallas poligonales, transformaciones geométricas, cámaras, iluminación y renderizado.

### Objetivos específicos

1. Generar una malla de terreno con relieve mediante ruido procedural controlado por una semilla numérica.
2. Dividir el terreno en dos tipos de zona, pradera y bosque, según los valores generados por el sistema.
3. Colocar árboles y rocas mediante reglas de posición, rotación, escala, pendiente y distancia mínima.
4. Implementar exploración en primera persona y una vista de depuración que muestre el mapa de alturas, las zonas y las cantidades de objetos generados.

## 5. Descripción técnica del proyecto

El producto será una aplicación 3D de escritorio desarrollada como un recorrido virtual. El mapa tendrá un tamaño aproximado de 128 × 128 metros y límites definidos. El usuario podrá desplazarse con el teclado y orientar una cámara en primera persona mediante el ratón. Se utilizarán colisiones básicas para mantener al usuario sobre el terreno y dentro del área de exploración.

Al iniciar la aplicación, una semilla numérica controlará la generación del escenario. El mismo valor deberá producir siempre los mismos resultados. El terreno se construirá como una malla compuesta por vértices y triángulos. La altura de sus vértices se calculará mediante una combinación sencilla de capas de ruido procedural. También se generarán o calcularán las normales necesarias para que la superficie responda correctamente a la iluminación.

El escenario contendrá dos tipos de zona: pradera y bosque. La clasificación se realizará utilizando los valores de altura y un segundo mapa procedural de densidad. La pradera tendrá menor concentración de objetos, mientras que el bosque tendrá una mayor cantidad de árboles. No se buscará simular un ecosistema real; las reglas sólo deberán producir una distribución visualmente coherente y fácil de explicar.

Los elementos gráficos principales serán el terreno, dos modelos sencillos de árbol y uno o dos modelos de roca. Cada objeto podrá presentar variaciones de rotación y escala para evitar una apariencia completamente repetitiva. Antes de colocarlo, el sistema verificará que la pendiente sea adecuada y que no exista otro objeto grande demasiado cerca. Para conservar un desarrollo rápido, se empleará una cuadrícula o una lista de posiciones ocupadas en lugar de un sistema avanzado de distribución espacial.

La escena utilizará una luz direccional, iluminación ambiental, materiales de colores simples y sombras básicas. Unity administrará el renderizado, la cámara y las colisiones, mientras que el equipo implementará la generación del terreno, las semillas, la clasificación de zonas y la colocación de objetos. Una interfaz sencilla mostrará la semilla y el número de árboles y rocas. También permitirá cambiar entre la vista normal, el mapa de alturas y el mapa de zonas. El proyecto no incluirá agua, caminos, clima, ciclo de día y noche, partículas, shaders personalizados ni generación infinita.

## 6. Herramientas y tecnologías

- **Unity 6 en una versión LTS estable:** creación de la aplicación, escenas, cámara, iluminación, colisiones y renderizado.
- **Universal Render Pipeline (URP):** materiales, sombras e iluminación en tiempo real.
- **C#:** programación del terreno procedural, semilla, zonas, distribución de objetos, controles e interfaz.
- **Blender o primitivas de Unity:** creación de modelos low-poly sencillos para árboles y rocas.
- **Git y GitHub:** control de versiones y respaldo del proyecto.

## 7. Relación con los temas de la asignatura

| Tema de Graficación por Computadora | Aplicación dentro del proyecto |
|---|---|
| Espacio euclídeo y geometría afín | Representación de posiciones, direcciones, distancias y pendientes en el escenario. |
| Transformaciones geométricas | Traslación, rotación y escalamiento de árboles y rocas. |
| Representaciones matriciales y coordenadas homogéneas | Transformación de los objetos entre sus coordenadas locales, el mundo y la cámara. |
| Mallas poligonales | Construcción del terreno mediante vértices y triángulos. |
| Proyección en perspectiva | Visualización del entorno mediante una cámara en primera persona. |
| Detección de superficies visibles | Uso de profundidad y descarte de elementos que no son visibles para la cámara. |
| Modelos de color | Diferenciación visual de la pradera, el bosque, el terreno y los objetos. |
| Iluminación y sombreado | Aplicación de luz ambiental y direccional, normales, materiales y sombras básicas. |
| Renderizado | Integración de la geometría, cámara, materiales e iluminación para producir la imagen final. |
