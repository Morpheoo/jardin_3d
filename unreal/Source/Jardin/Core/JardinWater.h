// JardinWater.h — Lagos, mares e islas por Priority-Flood.
//
// C++ PLANO. Sin tipos de Unreal. Unidades en METROS; alturas normalizadas.
//
// El agua no se pone a una altura fija: se simula. Priority-Flood (Barnes,
// Lehman y Mulla, 2014) es un algoritmo de hidrologia de los SIG. Llena cada
// hondonada del terreno hasta el punto mas bajo de su borde, por donde se
// desbordaria. Asi aparecen lagos solo donde el relieve los permite, cada uno
// a su propio nivel.
//
// Idea del algoritmo: una cola de prioridad empieza con las celdas del borde
// del mapa (por ahi escapa el agua). Se saca siempre la celda MAS BAJA y se
// visitan sus vecinas: cada vecina queda a max(su altura, la de quien la
// alcanzo). Una hondonada rodeada de terreno alto se alcanza por su paso mas
// bajo y toda ella queda "rellena" a esa altura: esa es la superficie del lago.
//
// Si el borde se inicializa en un nivel del mar en vez de en su altura, el
// MISMO algoritmo produce el mar (todo lo conectado al borde por debajo de ese
// nivel) y los lagos interiores. Un solo algoritmo para todos los perfiles.
//
// DETERMINISMO: los empates de la cola se resuelven por indice de celda.

#pragma once

#include "JardinWorld.h"

namespace jardin
{

/** Regimen hidrico. La semilla lo sortea en modo automatico. */
enum class WaterProfile : uint8_t
{
    None = 0,
    Seco,          // casi nada: unas lagunas en lo mas hondo
    Estanques,     // unas pocas lagunas
    Lagos,         // todas las hondonadas llenas
    GranLago,      // un anillo de montes encierra un mar interior
    Archipielago,  // el mar sube hasta que las colinas quedan como islas
    Isla,          // el jardin entero rodeado de mar
    Count
};

const char* WaterProfileName(WaterProfile P);

namespace WaterGenerator
{
    /**
     * Elige el perfil (fijo o sorteado), excava si hace falta, inunda y llena
     * World.WaterLevel y World.WaterDistance. No hace nada si el modo es 0.
     *
     * Va DESPUES de la altura y ANTES de la humedad: el agua cambia la humedad
     * de su alrededor, y con ella las zonas y la vegetacion.
     */
    void Generate(WorldData& World, const GenerationSettings& S, const SeedBook& Seeds);

    /**
     * Priority-Flood. Devuelve la altura "rellena" de cada celda: >= su altura
     * real; mayor solo donde hay agua estancada. SeaLevel01 = nivel inicial del
     * borde (0 = sin mar). Expuesto para poder probarlo por separado.
     */
    std::vector<float> PriorityFlood(const WorldData& World, float SeaLevel01);
}

} // namespace jardin
