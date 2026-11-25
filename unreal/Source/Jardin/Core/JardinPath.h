// JardinPath.h — Camino por spline (Fase 4).
//
// C++ PLANO. Sin tipos de Unreal. Unidades en METROS.
//
// Tres piezas, cada una demostrable por separado:
//   1. Ruta      Dijkstra sobre una rejilla gruesa del terreno: esquiva el agua
//                y prefiere las pendientes suaves, como un sendero real.
//   2. Curva     Catmull-Rom centripeta propia (NO USplineComponent) a traves
//                de puntos de control tomados de la ruta. Se remuestrea por
//                longitud de arco: una muestra cada metro.
//   3. Terreno   la calzada se aplana a lo ancho y se funde con el terreno en
//                los arcenes; PathDistance alimenta a NoPathRule y al material.
//
// Y una cuarta, para dibujar: la cinta de ancho constante (BuildRibbon).

#pragma once

#include "JardinWorld.h"

namespace jardin
{

namespace CatmullRom
{
    struct Vec2 { float X = 0.0f, Z = 0.0f; };

    /**
     * Punto de la curva entre P1 y P2, con t en [0,1].
     *
     * Formulacion de Barry-Goldman (piramide de interpolaciones lineales) con
     * parametrizacion por nudos t_{i+1} = t_i + |P_{i+1} - P_i|^Alpha:
     *   Alpha = 0    uniforme     puede hacer bucles y picos en curvas cerradas
     *   Alpha = 0.5  centripeta   sin bucles ni autointersecciones  <- la usada
     *   Alpha = 1    cordal       mas tensa
     * Pasa EXACTAMENTE por P1 (t = 0) y por P2 (t = 1): es interpolante, a
     * diferencia de Bezier o B-spline, que solo se acercan a sus puntos.
     */
    Vec2 Evaluate(const Vec2& P0, const Vec2& P1, const Vec2& P2, const Vec2& P3,
                  float t, float Alpha = 0.5f);
}

namespace PathGenerator
{
    /**
     * Traza el camino y aplana el terreno bajo el. Llena World.Path y
     * World.PathDistance. No hace nada si S.PathMode es 0 o si no hay tierra
     * firme suficiente (un mundo casi todo agua).
     *
     * Va DESPUES del agua (necesita saber donde esta) y ANTES de la humedad y
     * la pendiente (que deben ver el terreno ya aplanado).
     */
    void Generate(WorldData& World, const GenerationSettings& S, const SeedBook& Seeds);
}

/** Malla de la cinta del camino. Posiciones en metros (X, Z horizontales, Y altura). */
struct RibbonMesh
{
    std::vector<float> PX, PZ, PY;   // posiciones
    std::vector<float> U, V;         // U: 0 borde izquierdo, 1 derecho; V: metros a lo largo / ancho
    int32_t Rows = 0;                // muestras; 2 vertices por fila: [2r] izquierdo, [2r+1] derecho

    int32_t Index(int32_t Col, int32_t Row) const { return Row * 2 + Col; }
};

namespace PathRibbon
{
    /**
     * Cinta de ancho constante: en cada muestra se desplaza el eje media anchura
     * a cada lado, en la direccion perpendicular a la tangente. Los vertices se
     * posan sobre el terreno ya aplanado, Lift metros por encima.
     *
     * Vertices con la misma convencion que la rejilla del terreno: columna 0 =
     * izquierda, 1 = derecha; fila = muestra. Asi el motor arma los triangulos
     * con el mismo codigo y el mismo sentido de giro que el terreno.
     */
    RibbonMesh Build(const WorldData& World, float Lift = 0.04f);
}

} // namespace jardin
