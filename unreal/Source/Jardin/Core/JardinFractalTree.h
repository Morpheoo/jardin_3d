// JardinFractalTree.h — Arbol fractal por ramificacion recursiva.
//
// C++ PLANO. Cero tipos de Unreal. Unidades en METROS. Espacio local Z-up:
// el arbol nace en el origen y crece hacia +Z.
//
// Es la pieza de geometria fractal del proyecto (Unidad III del temario).
// El resto del bosque usa modelos descargados e instanciados; esta familia se
// construye vertice a vertice por codigo, a partir de una regla recursiva:
//
//     rama(nivel) = segmento + N x rama(nivel + 1)     hasta la profundidad
//     rama(profundidad) = segmento + racimo de hojas
//
// Cada nivel reduce la longitud y el radio por un factor constante y abre un
// angulo respecto a la rama madre. Esa autosimilitud (cada rama es una copia
// reducida del arbol) es lo que lo hace fractal.
//
// DETERMINISMO: toda la aleatoriedad sale de un DeterministicRandom con la
// semilla recibida, consumido en un orden fijo (recorrido en profundidad).
// La misma semilla produce siempre el mismo arbol, vertice por vertice.
//
// CONVENCION DE CARAS: los triangulos se emiten en sentido antihorario visto
// desde fuera, con la regla de la mano derecha: (b - a) x (c - a) apunta
// hacia fuera. Es la convencion matematica habitual. Unreal usa la contraria,
// asi que la capa del motor invierte el orden al copiar (igual que el terreno).
// La prueba de Tests/HarnessFractal.cpp verifica esta convencion en cada cara.

#pragma once

#include "JardinCore.h"
#include <vector>

namespace jardin
{

// ---------------------------------------------------------------- vec3

struct Vec3
{
    float X = 0.0f, Y = 0.0f, Z = 0.0f;

    Vec3() = default;
    Vec3(float InX, float InY, float InZ) : X(InX), Y(InY), Z(InZ) {}

    Vec3 operator+(const Vec3& O) const { return Vec3(X + O.X, Y + O.Y, Z + O.Z); }
    Vec3 operator-(const Vec3& O) const { return Vec3(X - O.X, Y - O.Y, Z - O.Z); }
    Vec3 operator*(float S) const        { return Vec3(X * S, Y * S, Z * S); }

    float Dot(const Vec3& O) const { return X * O.X + Y * O.Y + Z * O.Z; }

    Vec3 Cross(const Vec3& O) const
    {
        return Vec3(Y * O.Z - Z * O.Y,
                    Z * O.X - X * O.Z,
                    X * O.Y - Y * O.X);
    }

    float Length() const { return std::sqrt(X * X + Y * Y + Z * Z); }

    Vec3 Normalized() const
    {
        const float L = Length();
        return L > 1e-8f ? Vec3(X / L, Y / L, Z / L) : Vec3(0.0f, 0.0f, 1.0f);
    }
};

// ---------------------------------------------------------------- params

struct FractalTreeParams
{
    /** Niveles de ramificacion, contando el tronco. 5 da un arbol legible. */
    int32_t Depth = 5;

    /** Ramas hijas en cada nudo. La primera continua casi recta (eje del
     *  arbol); las demas se abren. */
    int32_t Branches = 3;

    float TrunkLength = 3.0f;     // metros
    float TrunkRadius = 0.22f;    // metros

    /** Factor de reduccion por nivel: la razon de autosimilitud del fractal. */
    float LengthDecay = 0.72f;
    float RadiusDecay = 0.62f;

    /** Apertura de las ramas respecto a su madre, en grados. */
    float BranchAngleDeg = 34.0f;
    float AngleJitterDeg = 10.0f;

    /** Variacion aleatoria de longitud, como fraccion (0.15 = +-15%). */
    float LengthJitter = 0.15f;

    /** Tendencia de las ramas a crecer hacia arriba, buscando la luz. */
    float UpBias = 0.30f;

    /** Lados de cada rama. 6 da el aspecto low-poly del resto del jardin. */
    int32_t RadialSegments = 6;

    /** Tamano del racimo de hojas en las puntas, en metros. */
    float LeafSize = 0.85f;

    // --- forma (valores por defecto = arbol de la primera version) ---

    /** Tramos por rama. Con mas de 1 la rama se curva entre tramos. */
    int32_t SegmentsPerBranch = 1;

    /** Desviacion maxima entre un tramo y el siguiente, en grados. */
    float BendDeg = 0.0f;

    /** Donde nacen las ramas laterales a lo largo de la madre, en [0,1].
     *  1 = todas en la punta (paraguas); 0.5 = desde la mitad hacia arriba. */
    float BranchStart = 1.0f;

    /** Ensanchamiento de la base del tronco. 0.5 = 50% mas ancha. */
    float TrunkFlare = 0.0f;

    /** 0 = octaedro (puntiagudo); 1 = icosaedro deformado (redondeado). */
    int32_t LeafShape = 0;

    /** Cuantos de los ultimos niveles llevan hojas. 1 = solo las puntas. */
    int32_t LeafLevels = 1;

    /** Racimos por punta. Mas de 1 = copa mas densa. */
    int32_t LeavesPerTip = 1;

    /** Deformacion aleatoria de los racimos redondeados, en [0, 0.5]. */
    float LeafJitter = 0.0f;

    /** Acota a rangos seguros. El numero de ramas crece como Branches^Depth:
     *  sin limite, un valor tecleado por error congela el editor. */
    void Validate();
};

// ---------------------------------------------------------------- mesh

enum class TreePart : uint8_t { Wood = 0, Leaf = 1 };

struct TreeMesh
{
    std::vector<Vec3> Positions;
    std::vector<Vec3> Normals;

    /** Madera u hoja, por vertice. La capa del motor lo convierte en color. */
    std::vector<uint8_t> Part;

    /** 0 en la base del tronco, 1 en las puntas. Es el peso del viento de la
     *  Fase 2: el tronco no se mueve y las hojas si. Se guarda desde ahora
     *  porque recalcularlo despues obligaria a reconstruir el arbol. */
    std::vector<float> Flex;

    /** Indices de triangulos, antihorarios vistos desde fuera. */
    std::vector<int32_t> Triangles;

    int32_t BranchCount = 0;
    int32_t LeafCount = 0;
    float Height = 0.0f;

    int32_t TriangleCount() const { return static_cast<int32_t>(Triangles.size() / 3); }

    /** Huella del arbol, para las pruebas de determinismo. */
    uint32_t Checksum() const;
};

namespace FractalTree
{
    TreeMesh Build(FractalTreeParams Params, uint32_t Seed);

    /**
     * Variacion de forma entre ejemplares de una misma especie.
     *
     * Devuelve una copia de Base con su apertura, razon de reduccion,
     * fototropismo, altura y tamano de copa alterados segun Seed, y en algunos
     * ejemplares una rama mas por nudo. Amount = 0 devuelve Base sin cambios;
     * 1 es la variacion maxima. Determinista: misma semilla, misma forma.
     *
     * Sin esto, todos los arboles de una especie comparten la silueta y solo
     * difieren en el azar de sus ramas.
     */
    FractalTreeParams Vary(const FractalTreeParams& Base, uint32_t Seed, float Amount);

    /** Ramas que debe tener un arbol: 1 + B + B^2 + ... + B^(Depth-1). */
    int32_t ExpectedBranchCount(const FractalTreeParams& Params);

    /** Racimos que debe tener: LeavesPerTip por cada rama de los ultimos
     *  LeafLevels niveles. Con los valores por defecto, B^(Depth-1). */
    int32_t ExpectedLeafCount(const FractalTreeParams& Params);
}

} // namespace jardin
