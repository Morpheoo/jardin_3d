# Créditos y recursos externos

Todo el código de generación procedural (ruido, fBm, semillas, malla del terreno, zonas, colocación de objetos, árbol fractal, mapas de depuración) se escribió para este proyecto, sin bibliotecas de terceros, con Claude (Anthropic) bajo la dirección del autor. El detalle está en la sección «Uso de Herramientas de IA» del informe técnico.

Los siguientes recursos son externos y se usan conforme a su licencia.

## Modelos 3D y texturas

| Recurso | Autor | Licencia | Uso en el proyecto |
|---|---|---|---|
| Stylized Nature MegaKit (versión Standard, gratuita) | Quaternius — quaternius.com | **CC0 1.0** (dominio público) | Toda la vegetación y las rocas, colocadas por el algoritmo del proyecto |

Modelos usados (53), todos colocados por el algoritmo del proyecto: pinos, árboles comunes, árboles retorcidos de hoja roja, árboles secos, arbustos, rocas, piedras, helechos y plantas, flores y tréboles, hongos y pasto. La lista exacta está en `unreal/Scripts/construir_jardin.py` (tabla `CAPAS`).
Texturas (15): cortezas, hojas, flores, pasto, hongos y rocas del mismo kit.

Archivo original: `Stylized Nature MegaKit[Standard].zip`. Licencia incluida en `unreal/SourceArt/Quaternius_StylizedNature/License_Standard.txt`.

> CC0 no exige atribución, pero se acredita igualmente para dejar clara la frontera entre lo propio y lo externo.

## Motor y herramientas

- Unreal Engine 5.7 — Epic Games
- Visual Studio 2022 — Microsoft
