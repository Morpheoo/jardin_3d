# -*- coding: utf-8 -*-
"""
construir_jardin.py — Fase 1: ecosistema completo del jardin, construido por codigo.

Ejecutar en el editor de Unreal:  Tools -> Execute Python Script... -> este archivo

Es REPETIBLE: cambia un numero aqui y vuelve a ejecutarlo. Reconstruye los
materiales desde cero y reemplaza las capas del jardin. Lo que ya este
importado no se vuelve a importar.

Que hace, en orden:
  1. Importa las texturas y modelos del Stylized Nature MegaKit (Quaternius, CC0)
     que faltan, directamente desde SourceArt/. No hay que arrastrar nada.
  2. Crea 4 materiales maestros y 12 instancias, uno por familia de textura.
  3. Asigna a cada ranura de cada modelo su material, segun el nombre de la ranura.
  4. Reemplaza las capas del JardinWorldActor por 11 capas con variantes de malla.
  5. Regenera el jardin, guarda y muestra cuantos objetos puso cada capa.

LA VARIEDAD ES PROCEDURAL EN TRES NIVELES:
  - Que especie va en cada sitio: reglas de zona, pendiente y altura de cada capa.
  - Que modelo de esa especie: variante elegida por hash, determinista.
  - Que tono tiene ese ejemplar: nodo PerInstanceRandom en el material.
"""

import os
import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()

ROOT = "/Game/Nature"
F_TEX = ROOT + "/Texturas"
F_MESH = ROOT + "/Modelos"
F_MAT = ROOT + "/Materials"

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
KIT = os.path.join(PROJECT, "SourceArt", "Quaternius_StylizedNature")

# ======================================================================
#  CAPAS DEL JARDIN
#  Unidades en metros. Zonas: 0 = pradera, 1 = bosque, [] = todas.
#  Las capas se colocan de mayor a menor radio: los arboles primero, y lo
#  pequeno esquiva lo ya puesto.
#
#  max_count es un tope de seguridad. Si una capa lo alcanza, el script lo
#  avisa: el reparto recorre el mapa por filas y se cortaria de golpe.
# ======================================================================

# Relieve del terreno. Con todo en 0 el terreno es el de la Fase 0.
RELIEVE = dict(
    # Ancho de las colinas: ciclos a lo ancho del mapa. Medido con la semilla
    # 827461 en 1024 m (desnivel medio en ventanas de 50 m / terreno > 27 grados):
    #   1.6 ->  9 m /  6%   colinas de ~600 m: casi no se notan
    #   4.5 -> 15 m / 11%   colinas claras, casi todo caminable   <- elegido
    #   6.4 -> 19 m / 25%   mas dramatico, un cuarto del mapa sin vegetacion
    #  12.8 -> 27 m / 53%   mas de la mitad son laderas de roca
    frequency=4.5,
    # Medido con la semilla 827461 (p90 de altura / cima / desnivel en 50 m / caminable):
    #   80 m, formula anterior   38 m /  69 m / 14.7 m / 89%   "no convence"
    #  150 m, colinas + sierras  64 m / 115 m / 21.9 m / 80%   <- elegido
    max_height_meters=150.0,
    height_exponent=1.6,      # aplana el fondo de los valles de las colinas
    warp_strength=0.05,       # colinas curvas y alargadas en vez de redondas
    warp_frequency=3.0,
    ridge_weight=0.85,        # cordilleras con cresta que se SUMAN sobre las colinas...
    mountain_frequency=2.0,   # ...solo en algunas regiones del mapa
    terrace_weight=0.7,       # mesetas: plataformas elevadas de bordes abruptos
    terrace_steps=6.0,
    terrace_frequency=2.5,
    # Caracter del relieve por semilla (0 = todas iguales a lo de arriba).
    # Con 1 cada semilla sortea uno. Medido en 60 semillas (p90 de altura / roca):
    #   Llanura 21 m / 1%   Lomas 39 m / 21%   Mesetas 48 m / 17%
    #   Valles  55 m / 26%  Sierras 76 m / 43%  Picos  84 m / 62%
    relief_variation=1.0,
)

# ----------------------------------------------------------------------
#  Rendimiento. Para medir: en Play, tecla B (la camara recorre el camino y
#  al final muestra fps y tiempos de juego / render / GPU; tambien en el log).
#  Medir con False, luego con True, y comparar.
#    cull   cada capa deja de dibujarse a CORTE[capa] metros de la camara
#    lods   versiones simplificadas de cada modelo para cuando se ve de lejos
#           (LOD_NIVELES: % de triangulos que conserva y tamano en pantalla
#           a partir del cual se usa). False quita los LODs.
# ----------------------------------------------------------------------
OPTIMIZAR = dict(cull=True, lods=True)   # 1a medicion: asi. 2a: los dos en True
CORTE = {
    "Pasto": 80, "Flores": 90, "Piedras": 60, "Hongos": 60, "Helechos": 120,
    "Arbustos": 150, "Rocas": 300, "Luciernagas": 80,
}
LOD_NIVELES = [(1.0, 1.0), (0.5, 0.30), (0.2, 0.10)]

# ----------------------------------------------------------------------
#  Pasto denso (como en No Man's Sky): un tapiz de pasto SOLO alrededor de la
#  camara, por trozos de 16 m que aparecen al acercarse y se descartan al
#  alejarse. Cada trozo sale de su propia subsemilla: ir y volver da el mismo
#  pasto. Las matas lejanas encogen hasta desaparecer, cada una a su distancia
#  (no se ve un borde). Toma el color, las tonalidades y el viento del bioma.
#  Medido (nucleo): ~2.3 matas/m2 en pradera, ~0.65 en bosque; ~7 000 matas
#  en un radio de 45 m de media (hasta ~15 000 en pleno prado).
#  Solo en la partida: el viewport del editor no tiene camara de jugador.
# ----------------------------------------------------------------------
PASTO_DENSO = dict(
    activo=True,
    # v3: matas generadas por codigo (Core/JardinGrass: GrassClump), como el
    # pasto de No Man's Sky: briznas finas y rectas, raiz del color del suelo y
    # punta mas clara, normales hacia arriba (se ilumina como el terreno).
    # False = las matas del kit (mallas); se veian tumbadas al acortarlas.
    procedural=True,
    briznas=32,            # por mata (v3: 14; con LODs de lejos quedan 13 y 5, mas anchas)
    altura_brizna=0.20,    # metros (+-40% por brizna)
    radio_mata=0.22,       # metros (v3: 0.10: matitas sueltas, el suelo se veia desnudo)
    ancho_brizna=0.028,    # metros en la base (v4: 0.014: tan finas que se veia el suelo)
    variantes=4,           # matas distintas por jardin
    viento=6.0,            # cm que se mece la punta
    mallas=["Grass_Common_Short", "Grass_Wispy_Short"],
    radio=50.0,            # metros alrededor de la camara
    trozo=16.0,            # lado de cada trozo, metros
    densidad=7.0,          # candidatas por m2 (antes de manchas, zona y humedad). v1: 4, v2: 3, v3: 5
    bosque=0.45,           # en el bosque crece un 45% de lo que en la pradera (v3: 0.3)
    corte_mancha=0.30,     # mas alto = mas claros sin pasto. v2: 0.36
    escala=(0.7, 1.3),
    trozos_por_frame=4,    # trabajo repartido: sin tirones al caminar
)
# Altura del pasto (metros, a escala 1). Las matas del kit miden ~medio metro:
# la v1 llegaba a la rodilla. El material las aplasta hacia su base hasta esta
# altura sin cambiar su ancho (siguen cubriendo el suelo). Vale para el pasto
# denso y para la capa Pasto. "Que no pase de los talones": ~12 cm.
PASTO_ALTURA = 0.12

# ----------------------------------------------------------------------
#  Camino por spline (Fase 4). Dijkstra busca la ruta por tierra firme
#  esquivando agua y cuestas; Catmull-Rom centripeta la suaviza; la calzada
#  se aplana con pendiente maxima del 20% (desmonte y terraplen donde haga
#  falta). Medido en 30 semillas: todas con camino, ~1.3 km de media.
# ----------------------------------------------------------------------
CAMINO = dict(
    path_enabled=True,
    path_width_meters=3.0,            # ancho de la calzada
    path_shoulder_meters=2.5,         # arcen: donde el aplanado se funde con el terreno
    path_control_spacing_meters=40.0, # separacion inicial de los puntos de control
)
CAMINO_TIERRA = (0.30, 0.22, 0.14)    # calzada: tierra apisonada (lineal)
CAMINO_ARCEN = (0.26, 0.21, 0.15)     # tierra pisada del arcen, en el terreno

# ----------------------------------------------------------------------
#  Biomas de color (como los planetas de No Man's Sky). La semilla sortea
#  uno segun su peso. Los materiales conservan el sombreado de las texturas
#  y solo cambian el matiz; el agua no se toca.
#    follaje   copas de pinos y arboles comunes
#    pasto     pasto, arbustos y helechos; tambien el suelo de la pradera
#  Colores en hexadecimal sRGB, como en cualquier selector de color.
#  El suelo del bosque sale del follaje, mas oscuro (SUELO_BOSQUE).
#    cielo     color del cielo (None = el azul de la Tierra). Cambia la
#              dispersion de Rayleigh de la atmosfera: el horizonte, el
#              atardecer y la luz ambiental se tinen solos y con coherencia.
#              Los colores palidos dan cielos claros; los saturados, intensos.
#  "Verde" con fuerza 0 es el jardin original, sin recolorear.
#    primo     grados que gira el matiz el "color primo" (ver TONALIDADES).
#    agua      color del agua (hex). Sin la columna = el turquesa de MI_Agua.
#              El cielo ya se refleja en el agua (sobre todo de lado); esto es
#              el color del agua EN SI: el script lo traduce a absorcion (se
#              come los canales contrarios) y dispersion (velo en lo hondo).
# ----------------------------------------------------------------------
PALETAS = [
    dict(nombre="Verde",         peso=2.0, fuerza=0.0, follaje=None,      pasto=None, cielo=None, primo=-25),
    dict(nombre="Verde suave",   peso=1.0, fuerza=1.0, follaje="#8FC77E", pasto="#B7DE9C", cielo="#9FE3D2", primo=-25),
    dict(nombre="Morado",        peso=1.0, fuerza=1.0, follaje="#7B3FC0", pasto="#A47BDB", cielo="#F4A3C8", primo=30),
    dict(nombre="Rojo",          peso=1.0, fuerza=1.0, follaje="#B3222B", pasto="#CC4B3A", cielo="#FFB25E", primo=25),
    dict(nombre="Rosado fuerte", peso=1.0, fuerza=1.0, follaje="#E0308A", pasto="#F066AE", cielo="#B49CFF", primo=-25),
    dict(nombre="Rosa",          peso=1.0, fuerza=1.0, follaje="#F29CC0", pasto="#F7C0D6", cielo="#FFD0E4", primo=20),
    dict(nombre="Blanco",        peso=1.0, fuerza=1.0, follaje="#E6EBEE", pasto="#F3F3EE", cielo="#8EBBFF", primo=0),
    dict(nombre="Naranja",       peso=1.0, fuerza=1.0, follaje="#D9711C", pasto="#E99A3A", cielo="#FFD677", primo=20),
    dict(nombre="Amarillo",      peso=1.0, fuerza=1.0, follaje="#D7BE28", pasto="#E8D65E", cielo="#A6EBA0", primo=25),
    dict(nombre="Azul con rosa", peso=1.0, fuerza=1.0, follaje="#3A6CD8", pasto="#F08AC0", cielo="#E39BFF", primo=30),

    # v2 (8-oct): 14 mas, al estilo de los planetas de No Man's Sky. La regla
    # que mejor funciona: la vegetacion y el cielo en colores que se
    # COMPLEMENTAN (azul con cielo rosa, verde con cielo dorado), no del mismo
    # tono. El agua sigue turquesa en todas.
    dict(nombre="Azul rey",          peso=1.0, fuerza=1.0, follaje="#3F6FD1", pasto="#8DB4F0", cielo="#F7A8C8", primo=25, agua="#E890C0"),
    dict(nombre="Lavanda",           peso=1.0, fuerza=1.0, follaje="#7D5BC9", pasto="#B9A2E8", cielo="#FFD3A1", primo=-20),
    dict(nombre="Turquesa",          peso=1.0, fuerza=1.0, follaje="#1F9E95", pasto="#5FD3C2", cielo="#FFC2D9", primo=20),
    dict(nombre="Menta y coral",     peso=1.0, fuerza=1.0, follaje="#3DBF8A", pasto="#9BE6C0", cielo="#FF9C8A", primo=-30),
    dict(nombre="Otono",             peso=1.0, fuerza=1.0, follaje="#C2551E", pasto="#D9A03A", cielo="#9EC7FF", primo=-20),
    dict(nombre="Carmesi",           peso=1.0, fuerza=1.0, follaje="#8E1B3A", pasto="#C2456B", cielo="#7FA3FF", primo=20),
    dict(nombre="Dorado",            peso=1.0, fuerza=1.0, follaje="#C9A227", pasto="#E7D27A", cielo="#A9D6FF", primo=-25),
    dict(nombre="Cian electrico",    peso=1.0, fuerza=1.0, follaje="#00A6D6", pasto="#5FE0F5", cielo="#FFB3F0", primo=25),
    dict(nombre="Esmeralda",         peso=1.0, fuerza=1.0, follaje="#0E7A4C", pasto="#33B26E", cielo="#FFE08A", primo=-25),
    dict(nombre="Ceniza",            peso=1.0, fuerza=1.0, follaje="#7F8A99", pasto="#B7C0CC", cielo="#FFB89A", primo=15, agua="#7FA6C2"),
    dict(nombre="Magenta y lima",    peso=1.0, fuerza=1.0, follaje="#B0238F", pasto="#A6D93B", cielo="#9FD8FF", primo=30),
    dict(nombre="Durazno",           peso=1.0, fuerza=1.0, follaje="#F08A5D", pasto="#FFC59E", cielo="#A6B8FF", primo=20),
    dict(nombre="Indigo",            peso=1.0, fuerza=1.0, follaje="#2B2F8F", pasto="#6E6FD8", cielo="#FF9EC4", primo=25, agua="#B070E0"),
    dict(nombre="Rojo y cielo menta", peso=1.0, fuerza=1.0, follaje="#A3172B", pasto="#D04A3A", cielo="#A6EBC0", primo=20, agua="#3FD9A0"),

    # v3 (8-oct): con agua propia.
    dict(nombre="Carbon",            peso=1.0, fuerza=1.0, follaje="#2B2B2E", pasto="#4A4A50", cielo="#FF5A4A", primo=15, agua="#D8303A"),
    dict(nombre="Abismo",            peso=1.0, fuerza=1.0, follaje="#1B2A5C", pasto="#2F4A8A", cielo="#6CE0E0", primo=20, agua="#2A5BD8"),
    dict(nombre="Atardecer eterno",  peso=1.0, fuerza=1.0, follaje="#6B2D5C", pasto="#C46A8A", cielo="#FF8A3D", primo=-20, agua="#FF9E6B"),
    dict(nombre="Selva negra",       peso=1.0, fuerza=1.0, follaje="#10241A", pasto="#2E5A3A", cielo="#C9E86A", primo=-20, agua="#3FB37F"),
    dict(nombre="Algodon",           peso=1.0, fuerza=1.0, follaje="#F5C6E0", pasto="#FBE3F0", cielo="#9ED0FF", primo=20, agua="#B8F0FF"),
    dict(nombre="Ambar toxico",      peso=1.0, fuerza=1.0, follaje="#6E7A12", pasto="#B8C230", cielo="#E8A33A", primo=-25, agua="#9BE03A"),
]
# ----------------------------------------------------------------------
#  Tonalidades dentro de cada bioma (como en la Tierra: casi todo verde
#  fuerte, algunos arboles mas oscuros, otros palidos y otros amarillentos).
#  ARBOLES: cada ejemplar sortea su tono con su numero al azar.
#  PASTO y plantas: por MANCHAS de ~30 m (ruido en el espacio), no por
#  planta: asi salen parches de prado palido o amarillento, no un salpicado.
#  Cortes acumulados: 55% dominante, 20% oscuro, 15% palido, 10% primo.
#  "primo" = el color con el matiz girado (PALETAS, columna primo).
# ----------------------------------------------------------------------
TONALIDADES = dict(
    corte_oscuro=0.55, corte_palido=0.75, corte_primo=0.90,
    oscuro=0.62,          # multiplica el color
    palido=0.50,          # cuanto se acerca a un gris claro de su misma luz
    palido_luz=1.30,      # ...y cuanto se aclara
    primo_luz=1.05,
    jitter=0.16,          # +-8% de brillo dentro del tono dominante
    mancha_pasto=0.00035, # escala del ruido de manchas (1/cm): ~30 m
)

SUELO_BOSQUE = 0.45   # suelo del bosque = follaje x esto (en el original, ~0.45)
# -1 = la semilla elige. Un numero (0 = Verde, 2 = Morado...) fija la paleta
# para todas las semillas: util para capturas o para ajustar un color.
PALETA_FIJA = -1

# ----------------------------------------------------------------------
#  Agua. Priority-Flood llena las hondonadas del relieve; el regimen decide
#  cuanto. En "AUTOMATICO" la semilla lo sortea, con estos pesos:
#    Seco 10% | Estanques 25% | Lagos 30%          <- normales
#    Gran lago 15% | Archipielago 10% | Isla 10%   <- exagerados
#  Para ver uno concreto: "SECO", "ESTANQUES", "LAGOS", "GRAN_LAGO",
#  "ARCHIPIELAGO", "ISLA". "APAGADO" quita el agua.
# ----------------------------------------------------------------------
AGUA = "AUTOMATICO"

# True: cada vez que le das a Play sale un jardin nuevo (semilla al azar).
# False: Play siempre arranca con la semilla del editor (util para comparar
# o para la demostracion de reproducibilidad). La tecla N funciona igual.
SEMILLA_AL_AZAR = False

# Coeficientes POR METRO (Single Layer Water; el script los pasa a 1/cm, que
# es lo que usa el shader de Unreal: SingleLayerWaterShading.ush multiplica
# por la profundidad en centimetros).
#
# De donde sale el cyan: NO de la dispersion. En el shader, el agua profunda
# devuelve como mucho ~8% de la luz (fase isotropa 1/4pi): por fisica, el agua
# honda siempre se ve oscura. El turquesa caribeno es AGUA MUY CLARA SOBRE
# ARENA PALIDA: la luz baja, rebota en la arena y vuelve; por el camino el agua
# se come el rojo y deja pasar verde y azul. Por eso:
#   absorcion   baja, salvo en rojo: el fondo se ve, tenido de cyan.
#   dispersion  poca: un velo turquesa en lo hondo, sin enturbiar.
#   tinte       color extra al fondo visto a traves del agua (1 = ninguno).
AGUA_COLOR = dict(
    absorcion=(0.22, 0.028, 0.018),
    dispersion=(0.002, 0.012, 0.014),
    tinte=(0.70, 1.00, 1.05),
    oleaje=0.55,         # inclinacion de las ondas (0 = espejo)
    espuma=0.85,         # espuma en la orilla (0 = sin espuma)
    rugosidad=0.03,
)
ARENA = (0.42, 0.36, 0.25)          # playa

# ----------------------------------------------------------------------
#  Viento. Se calcula en la GPU (World Position Offset): cada vertice se
#  desplaza segun su altura sobre la base de la planta. La base no se mueve
#  y la punta es la que mas se mece. Hojas y troncos usan LA MISMA formula,
#  asi la copa nunca se despega del tronco.
#    altura   cm sobre la base a los que el balanceo llega a su maximo
#    fuerza   cm que se mueve la punta
#    aleteo   cm de temblor fino de cada hoja (solo follaje)
#  Todo queda como parametros de las instancias: se ajusta en vivo.
# ----------------------------------------------------------------------
VIENTO_DIRECCION = (0.80, 0.60)     # hacia donde sopla (X, Y), normalizado
VIENTO_VELOCIDAD = 1.3              # ritmo del balanceo
VIENTO_ARBOL = dict(altura=900.0, fuerza=28.0, aleteo=2.5)
VIENTO_POR_INSTANCIA = {
    "MI_Hojas_Plantas": dict(altura=120.0, fuerza=10.0, aleteo=2.0),   # arbustos, helechos
    # Pasto corto (PASTO_ALTURA): el viento se calcula sobre la mata sin
    # aplastar, asi que la fuerza se baja para que no se doble de mas.
    "MI_Pasto":         dict(altura=50.0,  fuerza=3.5,  aleteo=0.8),
    "MI_PastoDenso":    dict(altura=50.0,  fuerza=3.5,  aleteo=0.8),
    "MI_Flores":        dict(altura=40.0,  fuerza=6.0,  aleteo=1.0),
    "MI_Hongos":        dict(altura=50.0,  fuerza=0.0,  aleteo=0.0),   # los hongos no se mecen
}
# Arboles fractales: el peso sale del color de vertice R (0 base, 1 hojas).
VIENTO_FRACTAL = dict(fuerza=70.0, aleteo=4.0)    # son de 45 m: se mecen mas
FONDO_AGUA = (0.66, 0.62, 0.50)     # antes 0.78: la orilla salia blanquecina

# Los ajustes hechos a mano en MI_Agua (en el editor) se conservan al volver a
# correr el script, y se imprimen en el log para copiarlos aqui si gustan.
# False = volver a los valores de AGUA_COLOR.
CONSERVAR_AJUSTES_AGUA = True     # lecho bajo el agua: palido, para que el agua se vea cyan

# Ecologia de cada capa:
#   moist   rango de humedad tolerado. La humedad varia suave por el mapa, asi
#           que las especies se REEMPLAZAN gradualmente en vez de mezclarse.
#   cluster 0 = salpicado; 1 = manchas con borde definido (dthr es el umbral
#           de la mancha). Quita el aspecto "aleatorio".
#   canopy  es un arbol que da sombra.
#   near    (metros) solo crece a esa distancia de un arbol: sotobosque.
#   avoid   (metros) no crece a esa distancia de un arbol: claros soleados.

# Densidad v2 (medida con el nucleo fuera de Unreal, 5 semillas): en la v1
# solo el 6% del terreno transitable tenia un claro de 10 x 10 m sin plantas
# bajas: habia plantas "a cada rato". Arbustos, helechos, hongos, piedras y
# rocas pasan a MANCHAS mas grandes (dfreq menor), mas escasas (dthr mayor) y
# de borde definido (cluster mayor): matorrales con claros entre ellos.
# Resultado: 30% de claros, y ~5 M de triangulos menos. Arboles sin cambios.
CAPAS = [
    # Bosques humedos y continuos de pino.
    dict(name="Pinos", meshes=["Pine_1", "Pine_2", "Pine_3", "Pine_4", "Pine_5"],
         zones=[1], spacing=4.0, radius=2.2, dfreq=12.0, dthr=0.42, cluster=0.8, canopy=True,
         moist=(0.50, 1.0), slope=0.32, hmin=0.03, hmax=0.85, smin=0.80, smax=1.30, maxc=40000),

    # Humedad media: donde el pino ya no prospera.
    dict(name="ArbolesComunes", meshes=["CommonTree_1", "CommonTree_2", "CommonTree_3", "CommonTree_4", "CommonTree_5"],
         zones=[0, 1], spacing=7.0, radius=2.2, dfreq=16.0, dthr=0.50, cluster=0.7, canopy=True,
         moist=(0.30, 0.62), slope=0.30, hmin=0.03, hmax=0.75, smin=0.80, smax=1.25, maxc=20000),

    # Arboledas rojas compactas en la pradera seca.
    dict(name="ArbolesRojos", meshes=["TwistedTree_1", "TwistedTree_2", "TwistedTree_3", "TwistedTree_4", "TwistedTree_5"],
         zones=[0], spacing=12.0, radius=2.6, dfreq=6.0, dthr=0.60, cluster=1.0, canopy=True,
         moist=(0.0, 0.50), slope=0.28, hmin=0.05, hmax=0.60, smin=0.85, smax=1.20, maxc=5000),

    # Arriba los arboles se secan: solo por encima del 40% de la altura.
    dict(name="ArbolesSecos", meshes=["DeadTree_1", "DeadTree_2", "DeadTree_3", "DeadTree_4", "DeadTree_5"],
         zones=[], spacing=10.0, radius=2.0, dfreq=20.0, dthr=0.45, cluster=0.4, canopy=True,
         slope=0.45, hmin=0.40, hmax=1.00, smin=0.80, smax=1.20, maxc=5000),

    # Arbustos en el borde y bajo los arboles.
    dict(name="Arbustos", meshes=["Bush_Common", "Bush_Common_Flowers"],
         zones=[0, 1], spacing=5.0, radius=1.0, dfreq=14.0, dthr=0.58, cluster=0.85, near=8.0,   # v1: 30 / 0.50 / 0.4
         slope=0.40, hmin=0.02, hmax=0.90, smin=0.80, smax=1.30, maxc=30000),

    # Pedregales, no rocas sueltas por todas partes.
    dict(name="Rocas", meshes=["Rock_Medium_1", "Rock_Medium_2", "Rock_Medium_3"],
         zones=[], spacing=7.0, radius=1.0, dfreq=10.0, dthr=0.60, cluster=0.8,   # v1: 20 / 0.55 / 0.6
         slope=0.65, hmin=0.00, hmax=1.00, smin=0.60, smax=1.60, maxc=20000),

    # Sotobosque: helechos solo a la sombra y con humedad.
    dict(name="Helechos", meshes=["Fern_1", "Plant_1", "Plant_1_Big", "Plant_7", "Plant_7_Big"],
         zones=[1], spacing=3.0, radius=0.6, dfreq=18.0, dthr=0.56, cluster=0.85, near=5.0,   # v1: 40 / 0.40 / 0.3
         moist=(0.48, 1.0), slope=0.40, hmin=0.02, hmax=0.85, smin=0.80, smax=1.30, maxc=60000),

    # Praderas de flores en los claros soleados.
    dict(name="Flores", meshes=["Flower_3_Group", "Flower_4_Group", "Flower_3_Single", "Flower_4_Single", "Clover_1", "Clover_2"],
         zones=[0], spacing=2.5, radius=0.35, dfreq=40.0, dthr=0.55, cluster=1.0, avoid=6.0,
         moist=(0.28, 0.62), slope=0.28, hmin=0.02, hmax=0.80, smin=0.80, smax=1.30, maxc=80000),

    # Hongos al pie de los arboles, en lo humedo.
    dict(name="Hongos", meshes=["Mushroom_Common", "Mushroom_Laetiporus"],
         zones=[1], spacing=7.0, radius=0.3, dfreq=20.0, dthr=0.60, cluster=0.85, near=3.5,   # v1: 48 / 0.50 / 0.5
         moist=(0.55, 1.0), slope=0.30, hmin=0.02, hmax=0.85, smin=0.70, smax=1.40, maxc=20000),

    dict(name="Piedras", meshes=["Pebble_Round_1", "Pebble_Round_2", "Pebble_Round_3", "Pebble_Round_4", "Pebble_Round_5",
                                 "Pebble_Square_1", "Pebble_Square_2", "Pebble_Square_3", "Pebble_Square_4",
                                 "Pebble_Square_5", "Pebble_Square_6"],
         zones=[], spacing=5.0, radius=0.25, dfreq=12.0, dthr=0.62, cluster=0.9,   # v1: 40 / 0.55 / 0.6
         slope=0.60, hmin=0.00, hmax=1.00, smin=0.60, smax=1.50, maxc=60000),

    # Pasto al sol, en la pradera abierta.
    dict(name="Pasto", meshes=["Grass_Common_Short", "Grass_Common_Tall", "Grass_Wispy_Short", "Grass_Wispy_Tall"],
         zones=[0], spacing=1.6, radius=0.15, dfreq=24.0, dthr=0.30, cluster=0.3, avoid=2.5,
         slope=0.35, hmin=0.02, hmax=0.90, smin=0.80, smax=1.40, maxc=200000),

    # Luciernagas: solo de noche (las dibuja el actor si bIsNight; lo pone
    # atmosfera.py). Enjambres en lo humedo y junto al agua. Cada una es una
    # esferita emisiva; el vuelo y el parpadeo los calcula la GPU
    # (M_Luciernaga), con fase propia por ejemplar. "diametro" (metros) manda
    # sobre smin/smax: la escala se calcula midiendo la malla.
    # El radio es el menor de todas las capas: se colocan al final y no
    # mueven ningun otro objeto, de dia o de noche.
    # v2: dthr 0.55 -> 0.62 y tope 12000 -> 40000: con 12000 llegaba al tope y
    # el reparto (por filas) dejaba parte del mapa sin luciernagas.
    dict(name="Luciernagas", meshes=["SM_LuciernagaEsfera"], nocturna=True, agua=0.5, diametro=(0.035, 0.05),
         zones=[], spacing=4.0, radius=0.08, dfreq=14.0, dthr=0.62, cluster=0.7,
         moist=(0.45, 1.0), slope=0.50, hmin=0.0, hmax=0.75, smin=0.05, smax=0.07, maxc=40000),

    # ARBOLES FRACTALES: no usan modelos descargados. Cada uno se construye
    # por codigo con ramificacion recursiva y su propia subsemilla, y ademas
    # varia su forma (apertura, altura, copa, ramas por nudo): todos distintos.
    #
    # Tres especies con la MISMA regla recursiva y distintos parametros.
    # Cantidades medidas en 5 semillas con el relieve actual, lejos de los topes
    # (si se alcanzaran, el reparto recorre el mapa por filas y se cortaria).
    #
    # Arquetipo comun de cerezo (v2): ramas curvas en 3 tramos, laterales desde
    # la mitad de la madre, base ensanchada, racimos redondeados, copa ligera.
    # Corrige la v1: ramas de alambre, copa de paraguas, hojas de bipiramide.

    # Colosales: hitos del paisaje. ~45-50 m de alto, tronco de ~4 m.
    # Medido: 4-9 por jardin. Colision activada: son los unicos contra los que
    # de verdad se choca. Se colocan primero (radio mayor) y nada los invade.
    dict(name="ArbolesColosales", camino=4.0, meshes=[], hojas="Colosal", corteza="Colosal", colision=True,
         fractal=dict(depth=6, branches=3, trunk_length_meters=16.0, trunk_radius_meters=2.0,
                      length_decay=0.68, radius_decay=0.64,
                      branch_angle_degrees=48.0, angle_jitter_degrees=12.0, up_bias=0.10,
                      segments_per_branch=4, bend_degrees=10.0, branch_start=0.60, trunk_flare=0.7,
                      round_leaves=True, leaf_levels=2, leaves_per_tip=1, leaf_size_meters=3.0,
                      leaf_jitter=0.30, variation=0.4),
         # Magenta en las zonas SECAS. Medido junto con los carmesi: 3-5 por jardin.
         zones=[], spacing=140.0, radius=16.0, dfreq=4.0, dthr=0.40, canopy=True,
         moist=(0.0, 0.58), slope=0.18, hmin=0.03, hmax=0.45, smin=0.90, smax=1.10, maxc=30),

    # Colosales carmesi: en las zonas HUMEDAS, con copa mas ancha y extendida.
    # Misma logica que los cerezos: cada color tiene su territorio. Medido: 2-3.
    dict(name="ColosalesCarmesi", camino=4.0, meshes=[], hojas="ColosalCarmesi", corteza="Colosal", colision=True,
         fractal=dict(depth=6, branches=3, trunk_length_meters=15.0, trunk_radius_meters=2.0,
                      length_decay=0.68, radius_decay=0.64,
                      branch_angle_degrees=52.0, angle_jitter_degrees=12.0, up_bias=0.05,
                      segments_per_branch=4, bend_degrees=10.0, branch_start=0.60, trunk_flare=0.7,
                      round_leaves=True, leaf_levels=2, leaves_per_tip=1, leaf_size_meters=3.0,
                      leaf_jitter=0.30, variation=0.4),
         zones=[], spacing=140.0, radius=16.0, dfreq=4.0, dthr=0.50, canopy=True,
         moist=(0.55, 1.0), slope=0.18, hmin=0.03, hmax=0.45, smin=0.90, smax=1.10, maxc=30),

    # Cerezos rosa: arboledas en la pradera SECA. Medido: 74-90 por jardin.
    dict(name="CerezosRosa", meshes=[], hojas="Rosa", corteza="Normal", colision=False,
         fractal=dict(depth=5, branches=3, trunk_length_meters=3.0, trunk_radius_meters=0.42,
                      length_decay=0.74, radius_decay=0.68,
                      branch_angle_degrees=38.0, angle_jitter_degrees=12.0, up_bias=0.25,
                      segments_per_branch=3, bend_degrees=12.0, branch_start=0.55, trunk_flare=0.5,
                      round_leaves=True, leaf_levels=2, leaves_per_tip=1, leaf_size_meters=0.85,
                      leaf_jitter=0.30, variation=0.5),
         zones=[0], spacing=16.0, radius=3.5, dfreq=10.0, dthr=0.63, cluster=1.0, canopy=True,
         moist=(0.0, 0.56), slope=0.30, hmin=0.03, hmax=0.60, smin=1.00, smax=1.40, maxc=400),

    # Cerezos rojizos: copa mas abierta, en las zonas HUMEDAS. Al cruzar el
    # mapa se pasa de arboledas rosas a rojizas en vez de verlas mezcladas.
    # Medido: 46-122 por jardin.
    dict(name="CerezosRojizos", meshes=[], hojas="Rojizo", corteza="Normal", colision=False,
         fractal=dict(depth=5, branches=3, trunk_length_meters=3.0, trunk_radius_meters=0.42,
                      length_decay=0.74, radius_decay=0.68,
                      branch_angle_degrees=44.0, angle_jitter_degrees=12.0, up_bias=0.15,
                      segments_per_branch=3, bend_degrees=12.0, branch_start=0.55, trunk_flare=0.5,
                      round_leaves=True, leaf_levels=2, leaves_per_tip=1, leaf_size_meters=0.85,
                      leaf_jitter=0.30, variation=0.5),
         zones=[0, 1], spacing=16.0, radius=3.5, dfreq=10.0, dthr=0.67, cluster=1.0, canopy=True,
         moist=(0.52, 1.0), slope=0.30, hmin=0.03, hmax=0.60, smin=1.00, smax=1.40, maxc=400),
]

# ======================================================================
#  MATERIALES
#  Nombre de ranura del FBX -> (instancia, maestro, textura, normal, ajustes)
#  Colores lineales; >1 aclara. "tinte" multiplica la textura entera.
# ======================================================================

VERDE_A, VERDE_B = (0.80, 0.95, 0.70), (1.10, 1.02, 0.62)
NEUTRO_A, NEUTRO_B = (0.88, 0.88, 0.88), (1.10, 1.06, 1.00)

INSTANCIAS = {
    "MI_Hojas_Comun":      ("hojas",   "Leaves_NormalTree_C",  None, dict(a=VERDE_A, b=VERDE_B, bioma="follaje")),
    "MI_Hojas_Pino":       ("hojas",   "Leaf_Pine_C",          None, dict(a=VERDE_A, b=VERDE_B, bioma="follaje")),
    "MI_Hojas_Rojas":      ("hojas",   "Leaves_TwistedTree_C", None, dict(a=(0.85, 0.85, 0.85), b=(1.15, 1.05, 0.85))),
    "MI_Hojas_Plantas":    ("hojas",   "Leaves",               None, dict(a=VERDE_A, b=VERDE_B, bioma="pasto")),
    "MI_Flores":           ("hojas",   "Flowers",              None, dict(a=NEUTRO_A, b=NEUTRO_B)),
    # La textura del pasto es casi blanca: viene pensada para teñirse. Se tiñe
    # del mismo verde que la pradera del terreno para que se funda con el suelo.
    "MI_Pasto":            ("hojas",   "Grass",                None, dict(a=(0.85, 0.95, 0.80), b=(1.10, 1.05, 0.75), tinte=(0.36, 0.54, 0.16), bioma="pasto")),
    # El mismo pasto, con el desvanecido a lo lejos: lo usa el pasto denso.
    "MI_PastoDenso":       ("hojas",   "Grass",                None, dict(a=(0.85, 0.95, 0.80), b=(1.10, 1.05, 0.75), tinte=(0.36, 0.54, 0.16), bioma="pasto", desvanecer=True)),
    "MI_Hongos":           ("hojas",   "Mushrooms",            None, dict(a=NEUTRO_A, b=NEUTRO_B)),
    "MI_Corteza_Comun":    ("corteza", "Bark_NormalTree",      "Bark_NormalTree_Normal", {}),
    "MI_Corteza_Seca":     ("corteza", "Bark_DeadTree",        "Bark_DeadTree_Normal", {}),
    "MI_Corteza_Retorcida": ("corteza", "Bark_TwistedTree",    "Bark_TwistedTree_Normal", {}),
    "MI_Roca":             ("roca",    "Rocks_Diffuse",        None, dict(a=(0.80, 0.80, 0.80), b=(1.08, 1.04, 0.98))),
    "MI_Piedras":          ("roca",    "PathRocks_Diffuse",    None, dict(a=(0.80, 0.80, 0.80), b=(1.08, 1.04, 0.98))),
}

RANURA_A_INSTANCIA = {
    "bark_normaltree":    "MI_Corteza_Comun",
    "bark_deadtree":      "MI_Corteza_Seca",
    "bark_twistedtree":   "MI_Corteza_Retorcida",
    "leaves_normaltree":  "MI_Hojas_Comun",
    "leaves_pine":        "MI_Hojas_Pino",
    "leaves_twistedtree": "MI_Hojas_Rojas",
    "leaves":             "MI_Hojas_Plantas",
    "flowers":            "MI_Flores",
    "grass":              "MI_Pasto",
    "mushrooms":          "MI_Hongos",
    "rocks":              "MI_Roca",
    "pathrocks":          "MI_Piedras",
    "material":           "MI_Hojas_Rojas",   # cuerpo de Bush_Common, que usa hojas rojas
}

TERRENO_PRADERA = (0.30, 0.48, 0.14)
TERRENO_BOSQUE = (0.12, 0.24, 0.08)
# Antes (0.36, 0.34, 0.31): con el sol fuerte y las cordilleras parecia nieve.
TERRENO_ROCA = (0.17, 0.16, 0.14)
TERRENO_TIERRA = (0.32, 0.25, 0.15)
# La roca empieza donde la vegetacion deja de crecer (arboles <= 0.32, pasto <= 0.35).
# Antes empezaba en 0.15 (13 grados): el pasto crecia sobre "roca" en cualquier ladera suave.
PENDIENTE_ROCA_INICIO, PENDIENTE_ROCA_TOTAL = 0.30, 0.42
ALTURA_TIERRA = 0.08

# ======================================================================
#  utilidades
# ======================================================================

log_ok, log_warn, log_err = [], [], []


def ok(m):
    log_ok.append(m); unreal.log("[Jardin] " + m)


def warn(m):
    log_warn.append(m); unreal.log_warning("[Jardin] " + m)


def err(m):
    log_err.append(m); unreal.log_error("[Jardin] " + m)


def index_assets():
    """nombre -> {clase: ruta} de todo lo que hay bajo /Game/Nature."""
    idx = {}
    for ad in registry.get_assets_by_path(ROOT, recursive=True):
        name = str(ad.asset_name)
        cls = str(ad.asset_class_path.asset_name)
        idx.setdefault(name, {})[cls] = str(ad.package_name)
    return idx


def find(idx, names, cls):
    for n in names:
        if n in idx and cls in idx[n]:
            return eal.load_asset(idx[n][cls])
    return None


def import_file(src, folder, dest_name=None):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", src)
    t.set_editor_property("destination_path", folder)
    if dest_name:
        t.set_editor_property("destination_name", dest_name)
    t.set_editor_property("automated", True)
    t.set_editor_property("save", True)
    t.set_editor_property("replace_existing", False)
    asset_tools.import_asset_tasks([t])
    return list(t.get_editor_property("imported_object_paths"))


def node(mat, cls, x, y, **props):
    e = mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def link(a, a_out, b, b_in):
    if not mel.connect_material_expressions(a, a_out, b, b_in):
        raise RuntimeError("no se pudo conectar %s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))


def to_prop(e, out, prop):
    if not mel.connect_material_property(e, out, prop):
        raise RuntimeError("no se pudo conectar %s.%s a %s" % (e.get_name(), out, prop))


def lc(rgb):
    return unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0)


def vparam(mat, name, rgb, x, y):
    return node(mat, unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=lc(rgb))


def const(mat, v, x, y):
    return node(mat, unreal.MaterialExpressionConstant, x, y, r=v)


def hex_lin(h):
    """'#RRGGBB' (sRGB, como en un selector de color) -> RGB lineal, que es lo
    que esperan los materiales."""
    h = h.lstrip("#")
    out = []
    for i in (0, 2, 4):
        c = int(h[i:i + 2], 16) / 255.0
        out.append(c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4)
    return tuple(out)


MPC_PATH = F_MAT + "/MPC_Bioma"
MPC_ESCALARES = {"Recolor": 0.0, "Variedad": 1.0, "AguaBioma": 0.0}
MPC_VECTORES = {"Follaje": TERRENO_PRADERA, "Pasto": TERRENO_PRADERA,
                "Pradera": TERRENO_PRADERA, "Bosque": TERRENO_BOSQUE,
                # Cociente primo/base que calcula el C++ (1, 1, 1 = sin cambio).
                "PrimoFollaje": (1.0, 1.0, 1.0), "PrimoPasto": (1.0, 1.0, 1.0),
                # Agua del bioma (1/cm); solo cuenta si AguaBioma = 1.
                "AguaAbsorcion": tuple(v * 0.01 for v in AGUA_COLOR["absorcion"]),
                "AguaDispersion": tuple(v * 0.01 for v in AGUA_COLOR["dispersion"]),
                "AguaTinte": AGUA_COLOR["tinte"]}


def ensure_mpc():
    """MPC_Bioma: la coleccion que el actor rellena con la paleta sorteada.
    Se reutiliza si existe: cada parametro tiene un GUID y los materiales lo
    referencian por el; recrearla los dejaria huerfanos."""
    if eal.does_asset_exist(MPC_PATH):
        mpc = eal.load_asset(MPC_PATH)
    else:
        mpc = asset_tools.create_asset("MPC_Bioma", F_MAT, unreal.MaterialParameterCollection,
                                       unreal.MaterialParameterCollectionFactoryNew())

    def merge(prop, cls, wanted, conv):
        actuales = {str(p.get_editor_property("parameter_name")): p for p in mpc.get_editor_property(prop)}
        lista = []
        for nombre, valor in wanted.items():
            p = actuales.get(nombre) or cls()
            p.set_editor_property("parameter_name", nombre)
            p.set_editor_property("default_value", conv(valor))
            lista.append(p)
        mpc.set_editor_property(prop, lista)

    merge("scalar_parameters", unreal.CollectionScalarParameter, MPC_ESCALARES, float)
    merge("vector_parameters", unreal.CollectionVectorParameter, MPC_VECTORES, lc)
    eal.save_loaded_asset(mpc)
    return mpc


def mpc_node(mat, mpc, nombre, x, y, vector=True):
    """Lee un parametro de MPC_Bioma. Los vectores salen RGBA: se recortan a RGB
    para poder mezclarlos con colores de 3 canales."""
    e = node(mat, unreal.MaterialExpressionCollectionParameter, x, y,
             collection=mpc, parameter_name=nombre)
    if not vector:
        return e
    rgb = node(mat, unreal.MaterialExpressionComponentMask, x + 200, y, r=True, g=True, b=True, a=False)
    link(e, "", rgb, "")
    return rgb


def recreate_material(name):
    path = F_MAT + "/" + name
    if eal.does_asset_exist(path):
        mat = eal.load_asset(path)
        mel.delete_all_material_expressions(mat)
    else:
        mat = asset_tools.create_asset(name, F_MAT, unreal.Material, unreal.MaterialFactoryNew())
    try:
        mat.set_editor_property("used_with_instanced_static_meshes", True)
    except Exception:
        pass
    return mat


def _log_path():
    return os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_log_dir()), "Jardin.log")


def finish(mat):
    """Ordena, compila y guarda. Unreal reporta los materiales que no compilan
    como AVISO en el log (no como excepcion) y los sustituye por el damero
    gris: se lee el log de ESTA compilacion para avisar con un ERROR."""
    mel.layout_material_expressions(mat)
    try:
        inicio = os.path.getsize(_log_path())
    except Exception:
        inicio = None
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    if inicio is None:
        return
    try:
        nombre = mat.get_name()
        with open(_log_path(), encoding="utf-8", errors="ignore") as f:
            f.seek(inicio)
            lineas = f.read().splitlines()
        for i, l in enumerate(lineas):
            if nombre + ".uasset" in l and "Failed to compile" in l:
                motivo = [x.strip() for x in lineas[max(0, i - 5):i + 5] if x.startswith("\t")]
                err("%s NO compila (sale el damero gris). Motivo: %s"
                    % (nombre, " | ".join(sorted(set(motivo))[:3]) or "ver el log"))
                break
    except Exception as e:
        warn("No pude revisar el log de %s: %s" % (mat.get_name(), e))


def recreate_instance(name, parent):
    path = F_MAT + "/" + name
    if eal.does_asset_exist(path):
        mi = eal.load_asset(path)
    else:
        mi = asset_tools.create_asset(name, F_MAT, unreal.MaterialInstanceConstant,
                                      unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    return mi


def tone_nodes(mat, a_rgb, b_rgb, x, y):
    """lerp(TonoA, TonoB, PerInstanceRandom): un tono distinto por ejemplar."""
    a = vparam(mat, "TonoA", a_rgb, x, y)
    b = vparam(mat, "TonoB", b_rgb, x, y + 150)
    rnd = node(mat, unreal.MaterialExpressionPerInstanceRandom, x, y + 300)
    t = node(mat, unreal.MaterialExpressionLinearInterpolate, x + 300, y + 100)
    link(a, "", t, "A"); link(b, "", t, "B"); link(rnd, "", t, "Alpha")
    return t


# ======================================================================
#  1. texturas y modelos
# ======================================================================

TEXTURAS = sorted({v[1] for v in INSTANCIAS.values()} | {v[2] for v in INSTANCIAS.values() if v[2]})


def ensure_textures():
    idx = index_assets()
    out = {}
    for name in TEXTURAS:
        tex = find(idx, ["T_" + name, name], "Texture2D")
        if tex is None:
            src = os.path.join(KIT, "Textures", name + ".png")
            if not os.path.isfile(src):
                err("No existe la textura en SourceArt: " + src)
                continue
            import_file(src, F_TEX, "T_" + name)
            tex = eal.load_asset(F_TEX + "/T_" + name)
            if tex is None:
                err("No se pudo importar " + name)
                continue
            ok("Importada textura " + name)
        if name.endswith("_Normal"):
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property("srgb", False)
            eal.save_loaded_asset(tex)
        out[name] = tex
    return out


# Mallas que no vienen del kit: se copian del contenido del motor. Se prueba
# cada fuente en orden y se usa la primera que exista.
# v1 era un cubo (SM_Luciernaga): de cerca se veia cuadrado.
MALLAS_MOTOR = {
    "SM_LuciernagaEsfera": ["/Engine/ArtTools/RenderToTexture/Meshes/S_1_Unit_Sphere",
                            "/Engine/EngineMeshes/Sphere",
                            "/Engine/BasicShapes/Sphere"],
}
MALLAS_OBSOLETAS = [F_MESH + "/SM_Luciernaga"]


def ensure_meshes():
    wanted = sorted({m for c in CAPAS for m in c["meshes"]})
    out = {}
    imported = 0
    for name in wanted:
        if name in MALLAS_MOTOR:
            path = F_MESH + "/" + name
            if not eal.does_asset_exist(path):
                fuente = next((f for f in MALLAS_MOTOR[name] if eal.does_asset_exist(f)), None)
                if fuente is None:
                    err("No encuentro ninguna malla de motor para %s" % name)
                    continue
                eal.duplicate_asset(fuente, path)
                ok("%s copiada de %s" % (name, fuente))
            out[name] = eal.load_asset(path)
            continue
        idx = index_assets()
        mesh = find(idx, [name], "StaticMesh")
        if mesh is None:
            src = os.path.join(KIT, "FBX", name + ".fbx")
            if not os.path.isfile(src):
                err("No existe el modelo en SourceArt: " + src)
                continue
            unreal.log("[Jardin] Importando %s ..." % name)
            import_file(src, F_MESH)
            mesh = find(index_assets(), [name], "StaticMesh")
            if mesh is None:
                err("No se pudo importar " + name)
                continue
            imported += 1
        out[name] = mesh
    ok("Modelos listos: %d (%d importados ahora)" % (len(out), imported))
    return out


# ======================================================================
#  2. materiales
# ======================================================================

def recolor_nodes(mat, mpc, base, x, y):
    """Bioma de color: conserva la LUMINANCIA del color original (sombras,
    vetas de la textura, tono de cada ejemplar) y le pone el matiz de la paleta.

        luz      = luminancia(base) / LumaRef        (1 = un tono medio)
        bioma    = lerp(Follaje, Pasto, UsaPasto) * luz
        salida   = lerp(base, bioma, Recolor * Recolorable)

    Recolor viene de la paleta (0 en la verde original); Recolorable es de cada
    instancia (0 en flores y hongos, que conservan su color)."""
    def sparam(nombre, valor, px, py):
        return node(mat, unreal.MaterialExpressionScalarParameter, px, py,
                    parameter_name=nombre, default_value=valor)

    pesos = node(mat, unreal.MaterialExpressionConstant3Vector, x, y + 100,
                 constant=unreal.LinearColor(0.2126, 0.7152, 0.0722, 1.0))
    luma = node(mat, unreal.MaterialExpressionDotProduct, x + 200, y)
    link(base, "", luma, "A"); link(pesos, "", luma, "B")
    luz = node(mat, unreal.MaterialExpressionDivide, x + 400, y)
    link(luma, "", luz, "A"); link(sparam("LumaRef", 0.35, x + 200, y + 100), "", luz, "B")

    follaje = mpc_node(mat, mpc, "Follaje", x, y + 250)
    pasto = mpc_node(mat, mpc, "Pasto", x, y + 350)
    color = node(mat, unreal.MaterialExpressionLinearInterpolate, x + 450, y + 300)
    link(follaje, "", color, "A"); link(pasto, "", color, "B")
    usa_pasto = sparam("UsaPasto", 0.0, x + 200, y + 450)
    link(usa_pasto, "", color, "Alpha")

    bioma = node(mat, unreal.MaterialExpressionMultiply, x + 650, y + 150)
    link(color, "", bioma, "A"); link(luz, "", bioma, "B")

    fuerza = node(mat, unreal.MaterialExpressionMultiply, x + 650, y + 450)
    link(mpc_node(mat, mpc, "Recolor", x + 400, y + 550, vector=False), "", fuerza, "A")
    recolorable = sparam("Recolorable", 0.0, x + 400, y + 650)
    link(recolorable, "", fuerza, "B")

    out = node(mat, unreal.MaterialExpressionLinearInterpolate, x + 850, y + 100)
    link(base, "", out, "A"); link(bioma, "", out, "B"); link(fuerza, "", out, "Alpha")
    return out, usa_pasto, recolorable


def tonalidades_nodes(mat, mpc, base, usa_pasto, recolorable, x, y):
    """Tonalidades dentro del bioma. Un numero r en [0,1] elige el tono:

        r < corte_oscuro                 dominante, con +-8% de brillo
        corte_oscuro <= r < corte_palido mas oscuro
        corte_palido <= r < corte_primo  palido: hacia un gris claro de su luz
        r >= corte_primo                 primo: base x (primo / base), del C++

    r = numero al azar del ejemplar (arboles) o ruido en manchas de ~30 m
    (pasto y plantas, UsaPasto = 1). Todo con nodos estandar: el nodo Custom
    no recibe bien el numero por instancia (M_Luciernaga v1 no compilaba)."""
    T = TONALIDADES

    def mul(a, b, px, py, cb=None):
        n = node(mat, unreal.MaterialExpressionMultiply, px, py, **({"const_b": cb} if cb is not None else {}))
        link(a, "", n, "A")
        if b is not None:
            link(b, "", n, "B")
        return n

    def lerp(a, b, alpha, px, py):
        n = node(mat, unreal.MaterialExpressionLinearInterpolate, px, py)
        link(a, "", n, "A"); link(b, "", n, "B"); link(alpha, "", n, "Alpha")
        return n

    def escalon(r, corte, px, py):
        """1 si r >= corte, 0 si no (rampa de una milesima)."""
        sub = node(mat, unreal.MaterialExpressionSubtract, px, py, const_b=corte)
        link(r, "", sub, "A")
        sat = node(mat, unreal.MaterialExpressionSaturate, px + 300, py)
        link(mul(sub, None, px + 150, py, 1000.0), "", sat, "")
        return sat

    # --- r: por ejemplar o por mancha
    rnd = node(mat, unreal.MaterialExpressionPerInstanceRandom, x, y)
    ruido = node(mat, unreal.MaterialExpressionNoise, x, y + 120, scale=T["mancha_pasto"], levels=2,
                 output_min=-0.35, output_max=1.35)
    mancha = node(mat, unreal.MaterialExpressionSaturate, x + 200, y + 120)
    link(ruido, "", mancha, "")
    r = lerp(rnd, mancha, usa_pasto, x + 400, y + 60)

    # --- los cuatro tonos
    jit = node(mat, unreal.MaterialExpressionFrac, x + 600, y - 200)
    link(mul(r, None, x + 450, y - 200, 17.0), "", jit, "")
    jit_f = node(mat, unreal.MaterialExpressionAdd, x + 900, y - 200, const_b=1.0 - T["jitter"] * 0.5)
    link(mul(jit, None, x + 750, y - 200, T["jitter"]), "", jit_f, "A")
    dominante = mul(base, jit_f, x + 1100, y - 200)

    oscuro = mul(base, None, x + 1100, y - 50, T["oscuro"])

    pesos = node(mat, unreal.MaterialExpressionConstant3Vector, x + 600, y + 250,
                 constant=unreal.LinearColor(0.2126, 0.7152, 0.0722, 1.0))
    luz = node(mat, unreal.MaterialExpressionDotProduct, x + 800, y + 200)
    link(base, "", luz, "A"); link(pesos, "", luz, "B")
    gris = mul(luz, None, x + 950, y + 200, T["palido_luz"])
    a_gris = node(mat, unreal.MaterialExpressionLinearInterpolate, x + 1100, y + 100, const_alpha=T["palido"])
    link(base, "", a_gris, "A"); link(gris, "", a_gris, "B")
    palido = mul(a_gris, None, x + 1250, y + 100, 1.10)

    primo_f = mpc_node(mat, mpc, "PrimoFollaje", x + 600, y + 400)
    primo_p = mpc_node(mat, mpc, "PrimoPasto", x + 600, y + 500)
    cociente = lerp(primo_f, primo_p, usa_pasto, x + 900, y + 450)
    primo = mul(mul(base, cociente, x + 1100, y + 400), None, x + 1250, y + 400, T["primo_luz"])

    # --- eleccion por cortes
    c1 = lerp(dominante, oscuro, escalon(r, T["corte_oscuro"], x + 600, y + 700), x + 1500, y)
    c2 = lerp(c1, palido, escalon(r, T["corte_palido"], x + 600, y + 800), x + 1650, y + 100)
    c3 = lerp(c2, primo, escalon(r, T["corte_primo"], x + 600, y + 900), x + 1800, y + 200)

    # Solo en lo recolorable (arboles, plantas, pasto), y con la fuerza del bioma.
    fuerza = mul(mpc_node(mat, mpc, "Variedad", x + 1500, y + 600, vector=False), recolorable, x + 1750, y + 600)
    return lerp(base, c3, fuerza, x + 2000, y + 100)


def build_hojas(tex_default, mpc=None):
    mat = recreate_material("M_Hojas")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("opacity_mask_clip_value", 0.33)

    tex = node(mat, unreal.MaterialExpressionTextureSampleParameter2D, -900, 0,
               parameter_name="Textura", texture=tex_default)
    tinte = vparam(mat, "Tinte", (1.0, 1.0, 1.0), -900, 250)
    tono = tone_nodes(mat, VERDE_A, VERDE_B, -900, 450)

    m1 = node(mat, unreal.MaterialExpressionMultiply, -500, 100)
    link(tex, "RGB", m1, "A"); link(tinte, "", m1, "B")
    m2 = node(mat, unreal.MaterialExpressionMultiply, -250, 200)
    link(m1, "", m2, "A"); link(tono, "", m2, "B")

    color = m2
    if mpc:
        color, usa_pasto, recolorable = recolor_nodes(mat, mpc, m2, -250, -700)
        color = tonalidades_nodes(mat, mpc, color, usa_pasto, recolorable, 700, -1400)
    to_prop(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    viento = viento_wpo(mat, -2600, 800, VIENTO_ARBOL, aleteo=True)
    desvanecer_wpo(mat, viento, -2600, 2200)
    to_prop(tex, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
    to_prop(const(mat, 0.85, -250, 450), "", unreal.MaterialProperty.MP_ROUGHNESS)
    to_prop(const(mat, 0.25, -250, 550), "", unreal.MaterialProperty.MP_SPECULAR)
    finish(mat)
    return mat


def desvanecer_wpo(mat, viento, x, y):
    """Pasto denso: a lo lejos cada mata encoge hasta su punto de anclaje y
    desaparece. Cada una lo hace a su propia distancia (numero al azar del
    ejemplar), asi el borde del pasto no es una linea sino un degradado.

        f        = saturate((distancia - Inicio) / (Fin - Inicio))
        visible  = saturate(((1 - f) - azar) * 6)
        WPO     += (anclaje - vertice) * (1 - visible) * Desvanecer

    Desvanecer vale 0 en todas las instancias salvo MI_PastoDenso."""
    def sparam(nombre, valor, px, py):
        return node(mat, unreal.MaterialExpressionScalarParameter, px, py,
                    parameter_name=nombre, default_value=valor)

    cam = node(mat, unreal.MaterialExpressionCameraPositionWS, x, y)
    obj = node(mat, unreal.MaterialExpressionObjectPositionWS, x, y + 100)
    wp = node(mat, unreal.MaterialExpressionWorldPosition, x, y + 200)
    dist = node(mat, unreal.MaterialExpressionDistance, x + 250, y)
    link(cam, "", dist, "A"); link(obj, "", dist, "B")

    ini = sparam("DesvaneceInicio", 2500.0, x + 250, y + 150)
    fin = sparam("DesvaneceFin", 4500.0, x + 250, y + 250)
    num = node(mat, unreal.MaterialExpressionSubtract, x + 450, y)
    link(dist, "", num, "A"); link(ini, "", num, "B")
    den = node(mat, unreal.MaterialExpressionSubtract, x + 450, y + 150)
    link(fin, "", den, "A"); link(ini, "", den, "B")
    f0 = node(mat, unreal.MaterialExpressionDivide, x + 650, y)
    link(num, "", f0, "A"); link(den, "", f0, "B")
    f = node(mat, unreal.MaterialExpressionSaturate, x + 800, y)
    link(f0, "", f, "")
    cerca = node(mat, unreal.MaterialExpressionOneMinus, x + 950, y)
    link(f, "", cerca, "")
    rnd = node(mat, unreal.MaterialExpressionPerInstanceRandom, x + 950, y + 150)
    v0 = node(mat, unreal.MaterialExpressionSubtract, x + 1100, y)
    link(cerca, "", v0, "A"); link(rnd, "", v0, "B")
    v1 = node(mat, unreal.MaterialExpressionMultiply, x + 1250, y, const_b=6.0)
    link(v0, "", v1, "A")
    vis = node(mat, unreal.MaterialExpressionSaturate, x + 1400, y)
    link(v1, "", vis, "")
    oculto = node(mat, unreal.MaterialExpressionOneMinus, x + 1550, y)
    link(vis, "", oculto, "")
    k = node(mat, unreal.MaterialExpressionMultiply, x + 1700, y)
    link(oculto, "", k, "A"); link(sparam("Desvanecer", 0.0, x + 1550, y + 150), "", k, "B")

    hacia = node(mat, unreal.MaterialExpressionSubtract, x + 450, y + 350)
    link(obj, "", hacia, "A"); link(wp, "", hacia, "B")
    enc = node(mat, unreal.MaterialExpressionMultiply, x + 1850, y + 200)
    link(hacia, "", enc, "A"); link(k, "", enc, "B")

    # Aplastar: cada vertice baja hacia la base de SU mata (la posicion del
    # objeto es la de cada instancia cuando hay instancing).
    #   z += (base.z - vertice.z) * (1 - AlturaRelativa)
    # AlturaRelativa = 1 (sin cambio) salvo en el pasto.
    alt = sparam("AlturaRelativa", 1.0, x + 450, y + 550)
    resto = node(mat, unreal.MaterialExpressionOneMinus, x + 650, y + 550)
    link(alt, "", resto, "")
    soloz = node(mat, unreal.MaterialExpressionMultiply, x + 650, y + 450)
    link(hacia, "", soloz, "A")
    link(node(mat, unreal.MaterialExpressionConstant3Vector, x + 450, y + 450,
              constant=unreal.LinearColor(0.0, 0.0, 1.0, 1.0)), "", soloz, "B")
    aplastar = node(mat, unreal.MaterialExpressionMultiply, x + 850, y + 500)
    link(soloz, "", aplastar, "A"); link(resto, "", aplastar, "B")

    total0 = node(mat, unreal.MaterialExpressionAdd, x + 2050, y + 100)
    link(viento, "", total0, "A"); link(enc, "", total0, "B")
    total = node(mat, unreal.MaterialExpressionAdd, x + 2250, y + 200)
    link(total0, "", total, "A"); link(aplastar, "", total, "B")
    to_prop(total, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    return total


def build_corteza(tex_default, normal_default):
    mat = recreate_material("M_Corteza")
    t = node(mat, unreal.MaterialExpressionTextureSampleParameter2D, -900, 0,
             parameter_name="Textura", texture=tex_default)
    n = node(mat, unreal.MaterialExpressionTextureSampleParameter2D, -900, 300,
             parameter_name="Normal", texture=normal_default,
             sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    rnd = node(mat, unreal.MaterialExpressionPerInstanceRandom, -900, 600)
    brillo = node(mat, unreal.MaterialExpressionLinearInterpolate, -600, 550, const_a=0.80, const_b=1.10)
    link(rnd, "", brillo, "Alpha")
    base = node(mat, unreal.MaterialExpressionMultiply, -300, 100)
    link(t, "RGB", base, "A"); link(brillo, "", base, "B")

    to_prop(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    to_prop(n, "RGB", unreal.MaterialProperty.MP_NORMAL)
    to_prop(const(mat, 0.90, -300, 450), "", unreal.MaterialProperty.MP_ROUGHNESS)
    # Mismo viento que las hojas (sin aleteo): la copa se mueve con su tronco.
    viento_wpo(mat, -2600, 800, VIENTO_ARBOL, aleteo=False)
    finish(mat)
    return mat


def build_roca(tex_default):
    mat = recreate_material("M_Roca")
    t = node(mat, unreal.MaterialExpressionTextureSampleParameter2D, -900, 0,
             parameter_name="Textura", texture=tex_default)
    tono = tone_nodes(mat, (0.80, 0.80, 0.80), (1.08, 1.04, 0.98), -900, 300)
    base = node(mat, unreal.MaterialExpressionMultiply, -300, 100)
    link(t, "RGB", base, "A"); link(tono, "", base, "B")
    to_prop(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    to_prop(const(mat, 0.85, -300, 450), "", unreal.MaterialProperty.MP_ROUGHNESS)
    finish(mat)
    return mat


def viento_wpo(mat, x, y, defaults, peso=None, fase=None, aleteo=True):
    """Desplazamiento por viento con nodos estandar (sin nodo Custom: la
    posicion de mundo es de doble precision en UE 5.7 y los nodos estandar
    si la manejan).

      peso  = (altura sobre la base / Altura)^2, o el nodo dado (fractales)
      fase  = ola de rafaga que viaja por el campo + ritmo propio del ejemplar
      lado  = sin(t - rafaga) * 0.6 + sin(2.3 t + propio) * 0.25 + 0.35
              (el +0.35 inclina las plantas a favor del viento)
      WPO   = direccion * lado * Fuerza * peso  [+ aleteo a lo largo de la normal]
    """
    def sine(px, py):
        return node(mat, unreal.MaterialExpressionSine, px, py, period=6.2831853)

    def param(nombre, valor, px, py):
        return node(mat, unreal.MaterialExpressionScalarParameter, px, py,
                    parameter_name=nombre, default_value=valor)

    wp = node(mat, unreal.MaterialExpressionWorldPosition, x, y)
    op = node(mat, unreal.MaterialExpressionObjectPositionWS, x, y + 120)

    if peso is None:
        rel = node(mat, unreal.MaterialExpressionSubtract, x + 200, y)
        link(wp, "", rel, "A"); link(op, "", rel, "B")
        relz = node(mat, unreal.MaterialExpressionComponentMask, x + 380, y, r=False, g=False, b=True, a=False)
        link(rel, "", relz, "")
        div = node(mat, unreal.MaterialExpressionDivide, x + 560, y)
        link(relz, "", div, "A"); link(param("AlturaViento", defaults["altura"], x + 380, y + 80), "", div, "B")
        sat = node(mat, unreal.MaterialExpressionSaturate, x + 740, y)
        link(div, "", sat, "")
        peso = node(mat, unreal.MaterialExpressionMultiply, x + 900, y)
        link(sat, "", peso, "A"); link(sat, "", peso, "B")

    # Tiempo
    tm = node(mat, unreal.MaterialExpressionTime, x, y + 260)
    t = node(mat, unreal.MaterialExpressionMultiply, x + 200, y + 260)
    link(tm, "", t, "A"); link(param("VelocidadViento", VIENTO_VELOCIDAD, x, y + 340), "", t, "B")

    # Ola de rafaga: depende de la posicion del ejemplar a lo largo del viento.
    oxy = node(mat, unreal.MaterialExpressionComponentMask, x + 200, y + 120, r=True, g=True, b=False, a=False)
    link(op, "", oxy, "")
    dir2 = node(mat, unreal.MaterialExpressionConstant2Vector, x + 200, y + 200,
                r=VIENTO_DIRECCION[0], g=VIENTO_DIRECCION[1])
    dot = node(mat, unreal.MaterialExpressionDotProduct, x + 380, y + 140)
    link(oxy, "", dot, "A"); link(dir2, "", dot, "B")
    rafaga = node(mat, unreal.MaterialExpressionMultiply, x + 560, y + 140, const_b=0.0035)
    link(dot, "", rafaga, "A")

    # Ritmo propio: numero al azar por ejemplar (o por arbol fractal).
    if fase is None:
        fase = node(mat, unreal.MaterialExpressionPerInstanceRandom, x + 200, y + 420)
    propio = node(mat, unreal.MaterialExpressionMultiply, x + 380, y + 420, const_b=6.2831853)
    link(fase, "", propio, "A")

    a1in = node(mat, unreal.MaterialExpressionSubtract, x + 740, y + 200)
    link(t, "", a1in, "A"); link(rafaga, "", a1in, "B")
    a1 = sine(x + 900, y + 200)
    link(a1in, "", a1, "")
    t23 = node(mat, unreal.MaterialExpressionMultiply, x + 560, y + 300, const_b=2.3)
    link(t, "", t23, "A")
    a2in = node(mat, unreal.MaterialExpressionAdd, x + 740, y + 340)
    link(t23, "", a2in, "A"); link(propio, "", a2in, "B")
    a2 = sine(x + 900, y + 340)
    link(a2in, "", a2, "")
    a1w = node(mat, unreal.MaterialExpressionMultiply, x + 1060, y + 200, const_b=0.6)
    link(a1, "", a1w, "A")
    a2w = node(mat, unreal.MaterialExpressionMultiply, x + 1060, y + 340, const_b=0.25)
    link(a2, "", a2w, "A")
    lado0 = node(mat, unreal.MaterialExpressionAdd, x + 1220, y + 260)
    link(a1w, "", lado0, "A"); link(a2w, "", lado0, "B")
    lado = node(mat, unreal.MaterialExpressionAdd, x + 1380, y + 260, const_b=0.35)
    link(lado0, "", lado, "A")

    amp = node(mat, unreal.MaterialExpressionMultiply, x + 1540, y + 200)
    link(lado, "", amp, "A"); link(param("FuerzaViento", defaults["fuerza"], x + 1380, y + 120), "", amp, "B")
    amp2 = node(mat, unreal.MaterialExpressionMultiply, x + 1700, y + 200)
    link(amp, "", amp2, "A"); link(peso, "", amp2, "B")
    dir3 = node(mat, unreal.MaterialExpressionConstant3Vector, x + 1700, y + 60,
                constant=unreal.LinearColor(VIENTO_DIRECCION[0], VIENTO_DIRECCION[1], 0.0, 1.0))
    wpo = node(mat, unreal.MaterialExpressionMultiply, x + 1860, y + 120)
    link(dir3, "", wpo, "A"); link(amp2, "", wpo, "B")

    if aleteo:
        # Temblor fino de las hojas a lo largo de su normal, con fase segun la
        # posicion del vertice: hojas vecinas tiemblan desfasadas.
        dw = node(mat, unreal.MaterialExpressionDotProduct, x + 380, y + 560)
        link(wp, "", dw, "A")
        link(node(mat, unreal.MaterialExpressionConstant3Vector, x + 200, y + 600,
                  constant=unreal.LinearColor(0.031, 0.023, 0.041, 1.0)), "", dw, "B")
        t55 = node(mat, unreal.MaterialExpressionMultiply, x + 380, y + 680, const_b=5.5)
        link(t, "", t55, "A")
        fin = node(mat, unreal.MaterialExpressionAdd, x + 560, y + 600)
        link(dw, "", fin, "A"); link(t55, "", fin, "B")
        fs = sine(x + 740, y + 600)
        link(fin, "", fs, "")
        fa = node(mat, unreal.MaterialExpressionMultiply, x + 900, y + 600)
        link(fs, "", fa, "A"); link(param("AleteoViento", defaults["aleteo"], x + 740, y + 680), "", fa, "B")
        fp = node(mat, unreal.MaterialExpressionMultiply, x + 1060, y + 600)
        link(fa, "", fp, "A"); link(peso, "", fp, "B")
        nrm = node(mat, unreal.MaterialExpressionVertexNormalWS, x + 1060, y + 700)
        fv = node(mat, unreal.MaterialExpressionMultiply, x + 1220, y + 640)
        link(nrm, "", fv, "A"); link(fp, "", fv, "B")
        total = node(mat, unreal.MaterialExpressionAdd, x + 2020, y + 200)
        link(wpo, "", total, "A"); link(fv, "", total, "B")
        wpo = total

    to_prop(wpo, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    return wpo


def ramp(mat, src, src_out, start, end, x, y):
    """saturate((x - start) / (end - start))."""
    sub = node(mat, unreal.MaterialExpressionSubtract, x, y, const_b=start)
    link(src, src_out, sub, "A")
    mul = node(mat, unreal.MaterialExpressionMultiply, x + 180, y, const_b=1.0 / (end - start))
    link(sub, "", mul, "A")
    sat = node(mat, unreal.MaterialExpressionSaturate, x + 360, y)
    link(mul, "", sat, "")
    return sat


def build_terreno(mpc=None):
    """Color del terreno a partir de los campos del generador, sin texturas:
    zona -> humedad -> roca en pendientes -> tierra en lo bajo.
    Pradera y bosque salen de MPC_Bioma: cambian con la paleta de la semilla."""
    mat = recreate_material("M_Terreno")
    vc = node(mat, unreal.MaterialExpressionVertexColor, -1600, 0)
    uv1 = node(mat, unreal.MaterialExpressionTextureCoordinate, -1600, 400, coordinate_index=1)
    zona = node(mat, unreal.MaterialExpressionComponentMask, -1400, 400, r=True, g=False, b=False, a=False)
    link(uv1, "", zona, "")

    if mpc:
        pradera = mpc_node(mat, mpc, "Pradera", -1600, 600)
        bosque = mpc_node(mat, mpc, "Bosque", -1600, 750)
    else:
        pradera = vparam(mat, "Pradera", TERRENO_PRADERA, -1400, 600)
        bosque = vparam(mat, "Bosque", TERRENO_BOSQUE, -1400, 750)
    roca = vparam(mat, "RocaLadera", TERRENO_ROCA, -1400, 900)
    tierra = vparam(mat, "TierraBaja", TERRENO_TIERRA, -1400, 1050)

    # Manchas del prado: el mismo ruido (y la misma escala) que el pasto, asi
    # el suelo palido queda debajo del pasto palido.
    ruido = node(mat, unreal.MaterialExpressionNoise, -1800, 800, scale=TONALIDADES["mancha_pasto"],
                 levels=2, output_min=0.86, output_max=1.14)
    pradera_m = node(mat, unreal.MaterialExpressionMultiply, -1300, 650)
    link(pradera, "", pradera_m, "A"); link(ruido, "", pradera_m, "B")

    suelo = node(mat, unreal.MaterialExpressionLinearInterpolate, -1100, 600)
    link(pradera_m, "", suelo, "A"); link(bosque, "", suelo, "B"); link(zona, "", suelo, "Alpha")

    hum = node(mat, unreal.MaterialExpressionLinearInterpolate, -1100, 200, const_a=1.10, const_b=0.80)
    link(vc, "B", hum, "Alpha")
    suelo_h = node(mat, unreal.MaterialExpressionMultiply, -900, 500)
    link(suelo, "", suelo_h, "A"); link(hum, "", suelo_h, "B")

    t_pend = ramp(mat, vc, "G", PENDIENTE_ROCA_INICIO, PENDIENTE_ROCA_TOTAL, -1300, -200)
    c1 = node(mat, unreal.MaterialExpressionLinearInterpolate, -600, 500)
    link(suelo_h, "", c1, "A"); link(roca, "", c1, "B"); link(t_pend, "", c1, "Alpha")

    t_alto = ramp(mat, vc, "R", 0.0, ALTURA_TIERRA, -1300, -400)
    t_bajo = node(mat, unreal.MaterialExpressionOneMinus, -800, -400)
    link(t_alto, "", t_bajo, "")
    c2 = node(mat, unreal.MaterialExpressionLinearInterpolate, -350, 500)
    link(c1, "", c2, "A"); link(tierra, "", c2, "B"); link(t_bajo, "", c2, "Alpha")

    # Arena en la orilla y bajo el agua. El alfa del color de vertice es la
    # cercania al agua (0 en el agua, 1 a mas de 6 m); sin agua vale 1 y el
    # terreno queda como antes.
    # Bajo el agua (alfa = 0) el lecho es mas claro que la playa: esa arena
    # palida, vista a traves del agua, es lo que se ve turquesa.
    playa = vparam(mat, "Arena", ARENA, -700, 1100)
    lecho = vparam(mat, "FondoAgua", FONDO_AGUA, -700, 1250)
    t_lecho = node(mat, unreal.MaterialExpressionMultiply, -550, 1350, const_b=8.0)
    link(vc, "A", t_lecho, "A")
    t_lecho_s = node(mat, unreal.MaterialExpressionSaturate, -450, 1350)
    link(t_lecho, "", t_lecho_s, "")
    arena = node(mat, unreal.MaterialExpressionLinearInterpolate, -350, 1100)
    link(lecho, "", arena, "A"); link(playa, "", arena, "B"); link(t_lecho_s, "", arena, "Alpha")
    c3 = node(mat, unreal.MaterialExpressionLinearInterpolate, -150, 500)
    link(arena, "", c3, "A"); link(c2, "", c3, "B"); link(vc, "A", c3, "Alpha")
    rug = node(mat, unreal.MaterialExpressionLinearInterpolate, -150, 800, const_a=0.55, const_b=0.95)
    link(vc, "A", rug, "Alpha")

    # Camino: UV2.x es 1 en la calzada y 0 al final del arcen (lo escribe el
    # C++). Ahi el suelo es tierra pisada. Sin camino, UV2 vale 0.
    uv2 = node(mat, unreal.MaterialExpressionTextureCoordinate, -350, 1500, coordinate_index=2)
    pisada = node(mat, unreal.MaterialExpressionComponentMask, -150, 1500, r=True, g=False, b=False, a=False)
    link(uv2, "", pisada, "")
    c4 = node(mat, unreal.MaterialExpressionLinearInterpolate, 50, 600)
    link(c3, "", c4, "A"); link(vparam(mat, "TierraCamino", CAMINO_ARCEN, -150, 1650), "", c4, "B")
    link(pisada, "", c4, "Alpha")

    to_prop(c4, "", unreal.MaterialProperty.MP_BASE_COLOR)
    to_prop(rug, "", unreal.MaterialProperty.MP_ROUGHNESS)
    to_prop(const(mat, 0.20, -350, 900), "", unreal.MaterialProperty.MP_SPECULAR)
    finish(mat)
    return mat


# Normal del agua: suma de ondas sinusoidales en varias direcciones, con su
# derivada exacta (no hace falta ninguna textura). Cada onda viaja a la
# velocidad que le corresponde por su longitud (dispersion en aguas profundas,
# w = sqrt(g k)), asi las largas avanzan mas rapido que las cortas como en un
# lago de verdad. Las ondas finas se apagan a lo lejos para no centellear.
# Devuelve la normal en espacio de mundo.
#
# P llega en METROS desde el UV0 de la malla del agua (lo escribe el C++). No
# se usa WorldPosition: en UE 5.7 es de doble precision y el nodo Custom no lo
# acepta ("missing input WP"); el material fallaba y salia el damero gris.
AGUA_HLSL = """
float2 p = P;
float2 g = float2(0.0, 0.0);
float2 D[7] = { float2(0.80, 0.60), float2(-0.45, 0.89), float2(0.97, -0.24),
                float2(-0.70, -0.71), float2(0.20, 0.98), float2(-0.95, 0.30),
                float2(0.55, -0.83) };
float L[7]  = { 9.0, 5.3, 3.1, 1.9, 1.2, 0.71, 0.43 };
float St[7] = { 0.055, 0.050, 0.045, 0.040, 0.035, 0.030, 0.025 };
float fade = saturate(1.0 - Dist / 15000.0);
for (int i = 0; i < 7; ++i)
{
    float k = 6.2831853 / L[i];
    float w = sqrt(9.81 * k);
    // Romper la reticula: cada onda se desfasa y cambia de fuerza segun la
    // posicion, con variaciones lentas (decenas de metros). Sin esto, 7 ondas
    // fijas se repiten y desde lejos se ve un patron de cuadricula.
    float2 q = p * (0.021 + 0.004 * i) + float2(i * 3.1, i * 1.3);
    float desfase = 2.5 * sin(q.x + 1.7 * sin(q.y)) + 2.0 * sin(q.y * 1.3 - 0.9 * sin(q.x * 0.8));
    float envolvente = 0.45 + 0.55 * saturate(0.5 + 0.5 * sin(q.x * 0.7 + i) * sin(q.y * 0.9 - i));
    float ph = dot(D[i], p) * k - w * T + i * 1.7 + desfase;
    float att = (i < 3) ? 1.0 : fade;
    g += D[i] * St[i] * cos(ph) * att * envolvente;
}
g *= S;
return normalize(float3(-g.x, -g.y, 1.0));
"""


# Espuma de orilla. D = profundidad (color de vertice R, 0..1 = 0..10 m; la
# escribe el C++). Una linea fija pegada a la arena, mas franjas que avanzan
# hacia la playa y se deshacen con un ruido barato, como olas pequenas.
ESPUMA_HLSL = """
float depth = D * 10.0;
float edge = 1.0 - saturate(depth / 0.8);
float n = sin(P.x * 1.7 + sin(P.y * 1.3 + T * 0.2)) * sin(P.y * 1.9 + sin(P.x * 0.9 - T * 0.3));
float franjas = 0.5 + 0.5 * sin(depth * 9.0 - T * 1.6 + n * 2.0);
float foam = edge * edge * saturate(franjas * 1.5 - 0.35 + n * 0.35);
foam = max(foam, pow(edge, 7.0));
return saturate(foam) * A;
"""


def build_agua(mpc=None):
    """Agua cristalina con el modelo Single Layer Water de Unreal: el motor
    calcula la absorcion y la dispersion segun la profundidad real de cada
    pixel, refracta el fondo y refleja el cielo."""
    # El agua se crea DESDE CERO cada vez. Reutilizar el material y borrarle
    # los nodos deja huerfano el nodo de salida del agua: el material compila
    # ese nodo viejo, sin entradas, y sale el damero gris del material por
    # defecto ("No inputs to Single Layer Water Material").
    # Leer los ajustes manuales de MI_Agua antes de borrarla.
    ajustes = {}
    mi_path = F_MAT + "/MI_Agua"
    if CONSERVAR_AJUSTES_AGUA and eal.does_asset_exist(mi_path):
        # Se compara cada valor de la instancia con el del material padre: solo
        # los que difieren son ajustes hechos a mano. Asi, los cambios futuros
        # en AGUA_COLOR siguen llegando a los parametros que nadie toco.
        try:
            viejo = eal.load_asset(mi_path)
            padre = viejo.get_editor_property("parent")
            for n in mel.get_scalar_parameter_names(viejo):
                n = str(n)
                v = mel.get_material_instance_scalar_parameter_value(viejo, n)
                if abs(v - mel.get_material_default_scalar_parameter_value(padre, n)) > 1e-5:
                    ajustes[n] = ("s", round(v, 4))
            for n in mel.get_vector_parameter_names(viejo):
                n = str(n)
                c = mel.get_material_instance_vector_parameter_value(viejo, n)
                d = mel.get_material_default_vector_parameter_value(padre, n)
                if max(abs(c.r - d.r), abs(c.g - d.g), abs(c.b - d.b)) > 1e-5:
                    ajustes[n] = ("v", (round(c.r, 4), round(c.g, 4), round(c.b, 4)))
        except Exception as e:
            warn("No pude leer los ajustes de MI_Agua: %s" % e)

    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in sub.get_all_level_actors():
        if isinstance(a, unreal.JardinWorldActor):
            a.set_editor_property("water_material", None)
    for n in ("MI_Agua", "M_Agua"):
        path = F_MAT + "/" + n
        if eal.does_asset_exist(path) and not eal.delete_asset(path):
            warn("No pude borrar %s; si el agua sale gris, borralo a mano y repite" % n)


    mat = recreate_material("M_Agua")
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SINGLE_LAYER_WATER)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("tangent_space_normal", False)
    mat.set_editor_property("two_sided", True)    # tambien se ve desde abajo

    wp = node(mat, unreal.MaterialExpressionWorldPosition, -1200, 0)
    tm = node(mat, unreal.MaterialExpressionTime, -1200, 150)
    cam = node(mat, unreal.MaterialExpressionCameraPositionWS, -1200, 300)
    dist = node(mat, unreal.MaterialExpressionDistance, -1000, 250)
    link(wp, "", dist, "A"); link(cam, "", dist, "B")
    ole = node(mat, unreal.MaterialExpressionScalarParameter, -1200, 450,
               parameter_name="Oleaje", default_value=AGUA_COLOR["oleaje"])

    cu = node(mat, unreal.MaterialExpressionCustom, -700, 150)
    cu.set_editor_property("code", AGUA_HLSL)
    cu.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    cu.set_editor_property("description", "OndasAgua")
    ins = []
    for n in ("P", "T", "S", "Dist"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    cu.set_editor_property("inputs", ins)
    uv = node(mat, unreal.MaterialExpressionTextureCoordinate, -1200, -150, coordinate_index=0)
    link(uv, "", cu, "P"); link(tm, "", cu, "T"); link(ole, "", cu, "S"); link(dist, "", cu, "Dist")

    to_prop(cu, "", unreal.MaterialProperty.MP_NORMAL)

    # Espuma: color base blanco tapando el agua donde hay espuma. El shader
    # multiplica el color base por Opacity (DiffuseColor *= Opacity) y el agua
    # por 1 - Opacity: con Opacity = mascara de espuma, fuera de ella el agua
    # queda intacta.
    vcol = node(mat, unreal.MaterialExpressionVertexColor, -1200, 600)
    esp = node(mat, unreal.MaterialExpressionScalarParameter, -1200, 750,
               parameter_name="Espuma", default_value=AGUA_COLOR["espuma"])
    cf = node(mat, unreal.MaterialExpressionCustom, -700, 600)
    cf.set_editor_property("code", ESPUMA_HLSL)
    cf.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    cf.set_editor_property("description", "EspumaOrilla")
    ins = []
    for n in ("D", "P", "T", "A"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    cf.set_editor_property("inputs", ins)
    link(vcol, "R", cf, "D"); link(uv, "", cf, "P"); link(tm, "", cf, "T"); link(esp, "", cf, "A")
    blanco = node(mat, unreal.MaterialExpressionMultiply, -350, 380, const_b=0.9)
    link(cf, "", blanco, "A")
    to_prop(blanco, "", unreal.MaterialProperty.MP_BASE_COLOR)
    to_prop(const(mat, 0.0, -300, 450), "", unreal.MaterialProperty.MP_METALLIC)
    # OJO: en Single Layer Water, Opacity es cuanto TAPA un material opaco al
    # agua (BasePassPixelShader.usf: WaterVisibility = 1 - Opacity). Sin
    # conectar vale 1: el agua no se dibuja y queda una superficie negra que
    # solo refleja el cielo. Tiene que ser 0 salvo donde hay espuma.
    to_prop(cf, "", unreal.MaterialProperty.MP_OPACITY)
    to_prop(const(mat, 0.5, -300, 500), "", unreal.MaterialProperty.MP_SPECULAR)
    rug = node(mat, unreal.MaterialExpressionScalarParameter, -300, 550,
               parameter_name="Rugosidad", default_value=AGUA_COLOR["rugosidad"])
    to_prop(rug, "", unreal.MaterialProperty.MP_ROUGHNESS)
    try:
        to_prop(const(mat, 1.33, -300, 650), "", unreal.MaterialProperty.MP_REFRACTION)
    except Exception as e:
        warn("M_Agua: sin refraccion (%s)" % e)

    out = node(mat, unreal.MaterialExpressionSingleLayerWaterMaterialOutput, 0, 900)
    por_cm = lambda c: tuple(v * 0.01 for v in c)
    abs_ = vparam(mat, "Absorcion", por_cm(AGUA_COLOR["absorcion"]), -700, 850)
    dis = vparam(mat, "Dispersion", por_cm(AGUA_COLOR["dispersion"]), -700, 1000)
    tin = vparam(mat, "TinteFondo", AGUA_COLOR["tinte"], -700, 1250)
    if mpc:
        # Agua del bioma: con AguaBioma = 1 (el bioma trae agua propia) manda
        # la MPC; con 0, los parametros de MI_Agua (el turquesa de siempre).
        bioma = mpc_node(mat, mpc, "AguaBioma", -900, 1400, vector=False)

        def del_bioma(local, nombre, py):
            mezcla = node(mat, unreal.MaterialExpressionLinearInterpolate, -400, py)
            link(local, "", mezcla, "A")
            link(mpc_node(mat, mpc, nombre, -900, py), "", mezcla, "B")
            link(bioma, "", mezcla, "Alpha")
            return mezcla
        abs_ = del_bioma(abs_, "AguaAbsorcion", 850)
        dis = del_bioma(dis, "AguaDispersion", 1000)
        tin = del_bioma(tin, "AguaTinte", 1250)
    pares = ((dis, ("ScatteringCoefficients", "Scattering Coefficients", "Scattering")),
             (abs_, ("AbsorptionCoefficients", "Absorption Coefficients", "Absorption")),
             (const(mat, 0.25, -400, 1150), ("PhaseG", "Phase G")),
             (tin, ("ColorScaleBehindWater", "Color Scale Behind Water")))
    for src, nombres in pares:
        if not any(mel.connect_material_expressions(src, "", out, n) for n in nombres):
            warn("M_Agua: no pude conectar %s; queda con el valor por defecto" % nombres[0])

    # finish() revisa el log de la compilacion FINAL y avisa si no compila.
    # (Mientras se arma el grafo, cada conexion recompila un material a medias
    # que falla siempre: esos errores intermedios no cuentan.)
    finish(mat)

    # Instancia: sus parametros se editan con el juego corriendo y el cambio se
    # ve al instante, sin recompilar el material ni volver a correr el script.
    mi = recreate_instance("MI_Agua", mat)
    for nombre, (tipo, valor) in ajustes.items():
        if tipo == "s":
            mel.set_material_instance_scalar_parameter_value(mi, nombre, valor)
        else:
            mel.set_material_instance_vector_parameter_value(mi, nombre, lc(valor))
        unreal.log("[Jardin]   MI_Agua conserva tu ajuste: %s = %s" % (nombre, valor))
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi)
    return mi


def build_instances(tex, mpc=None):
    masters = {
        "hojas": build_hojas(tex["Leaves_NormalTree_C"], mpc),
        "corteza": build_corteza(tex["Bark_NormalTree"], tex["Bark_NormalTree_Normal"]),
        "roca": build_roca(tex["Rocks_Diffuse"]),
    }
    ok("Materiales maestros: M_Hojas, M_Corteza, M_Roca")

    out = {}
    for mi_name, (kind, tname, nname, opts) in INSTANCIAS.items():
        try:
            mi = recreate_instance(mi_name, masters[kind])
            mel.set_material_instance_texture_parameter_value(mi, "Textura", tex[tname])
            if nname:
                mel.set_material_instance_texture_parameter_value(mi, "Normal", tex[nname])
            if "a" in opts:
                mel.set_material_instance_vector_parameter_value(mi, "TonoA", lc(opts["a"]))
                mel.set_material_instance_vector_parameter_value(mi, "TonoB", lc(opts["b"]))
            if kind == "hojas":
                mel.set_material_instance_vector_parameter_value(mi, "Tinte", lc(opts.get("tinte", (1, 1, 1))))
                # Bioma: que parte de la paleta toma (o ninguna: flores, hongos).
                bioma = opts.get("bioma")
                mel.set_material_instance_scalar_parameter_value(mi, "Recolorable", 1.0 if bioma else 0.0)
                mel.set_material_instance_scalar_parameter_value(mi, "UsaPasto", 1.0 if bioma == "pasto" else 0.0)
                if opts.get("desvanecer"):
                    r = PASTO_DENSO["radio"] * 100.0
                    mel.set_material_instance_scalar_parameter_value(mi, "Desvanecer", 1.0)
                    mel.set_material_instance_scalar_parameter_value(mi, "DesvaneceInicio", r * 0.55)
                    mel.set_material_instance_scalar_parameter_value(mi, "DesvaneceFin", r)
            if mi_name in VIENTO_POR_INSTANCIA:
                v = VIENTO_POR_INSTANCIA[mi_name]
                mel.set_material_instance_scalar_parameter_value(mi, "AlturaViento", v["altura"])
                mel.set_material_instance_scalar_parameter_value(mi, "FuerzaViento", v["fuerza"])
                mel.set_material_instance_scalar_parameter_value(mi, "AleteoViento", v["aleteo"])
            mel.update_material_instance(mi)
            eal.save_loaded_asset(mi)
            out[mi_name] = mi
        except Exception as e:
            err("Instancia %s: %s" % (mi_name, e))
    ok("Instancias de material: %d" % len(out))
    return out


# ======================================================================
#  3. asignar materiales a los modelos
# ======================================================================

def normalize(s):
    s = s.lower().split(".")[0]
    while s and (s[-1].isdigit() or s[-1] == "_"):
        s = s[:-1]
    return s


def assign_materials(meshes, instances):
    unknown = set()
    for name, mesh in meshes.items():
        if name in MALLAS_MOTOR:
            continue   # su material lo pone su propia funcion (build_luciernaga)
        slots = mesh.get_editor_property("static_materials")
        changed = False
        for i, slot in enumerate(slots):
            keys = [normalize(str(slot.get_editor_property("material_slot_name")))]
            mi_old = slot.get_editor_property("material_interface")
            if mi_old:
                keys.append(normalize(mi_old.get_name()))
            target = next((RANURA_A_INSTANCIA[k] for k in keys if k in RANURA_A_INSTANCIA), None)
            if target and target in instances:
                mesh.set_material(i, instances[target])
                changed = True
            else:
                unknown.add("%s[%d]=%s" % (name, i, keys))
        if changed:
            eal.save_loaded_asset(mesh)
    if unknown:
        warn("Ranuras sin material conocido: " + "; ".join(sorted(unknown)))
    ok("Materiales asignados a %d modelos" % len(meshes))


# ======================================================================
#  4. configurar el jardin
# ======================================================================

# Coeficientes de Rayleigh de la Tierra (normalizados al azul, como los guarda
# Unreal) y el matiz de la niebla de dia de atmosfera.py.
RAYLEIGH_TIERRA = (0.175287, 0.409607, 1.0)
NIEBLA_TIERRA = (0.45, 0.62, 0.95)


def _luma(c):
    return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]


def rayleigh(color):
    """Color deseado del cielo -> coeficientes de Rayleigh, con la misma
    luminancia que los de la Tierra: el cielo cambia de matiz sin volverse
    mucho mas brillante ni mas oscuro (ni mas o menos brumoso)."""
    k = _luma(RAYLEIGH_TIERRA) / max(_luma(color), 1e-4)
    return tuple(ch * k for ch in color)


def build_camino():
    """Calzada de tierra apisonada. Sin texturas: un ruido en coordenadas de
    mundo le da manchas, y los bordes (U = 0 y 1 en la cinta) se oscurecen un
    poco, como las rodadas del borde de un sendero."""
    mat = recreate_material("M_Camino")
    base = vparam(mat, "Tierra", CAMINO_TIERRA, -900, 0)

    # Sin entrada, el nodo Noise usa la posicion de mundo (v1 la conectaba a
    # mano y el pin no existia: M_Camino quedaba sin compilar).
    ruido = node(mat, unreal.MaterialExpressionNoise, -900, 300, scale=0.004, levels=3,
                 output_min=0.80, output_max=1.15)
    c1 = node(mat, unreal.MaterialExpressionMultiply, -600, 100)
    link(base, "", c1, "A"); link(ruido, "", c1, "B")

    # Bordes: 4 * u * (1 - u) vale 1 en el centro y 0 en los bordes.
    uv = node(mat, unreal.MaterialExpressionTextureCoordinate, -1100, 600, coordinate_index=0)
    u = node(mat, unreal.MaterialExpressionComponentMask, -900, 600, r=True, g=False, b=False, a=False)
    link(uv, "", u, "")
    um = node(mat, unreal.MaterialExpressionOneMinus, -750, 650)
    link(u, "", um, "")
    uu = node(mat, unreal.MaterialExpressionMultiply, -600, 600)
    link(u, "", uu, "A"); link(um, "", uu, "B")
    centro = node(mat, unreal.MaterialExpressionMultiply, -450, 600, const_b=4.0)
    link(uu, "", centro, "A")
    borde = node(mat, unreal.MaterialExpressionLinearInterpolate, -300, 600, const_a=0.80, const_b=1.0)
    link(centro, "", borde, "Alpha")
    c2 = node(mat, unreal.MaterialExpressionMultiply, -150, 200)
    link(c1, "", c2, "A"); link(borde, "", c2, "B")

    to_prop(c2, "", unreal.MaterialProperty.MP_BASE_COLOR)
    to_prop(const(mat, 0.95, -150, 400), "", unreal.MaterialProperty.MP_ROUGHNESS)
    to_prop(const(mat, 0.10, -150, 500), "", unreal.MaterialProperty.MP_SPECULAR)
    finish(mat)
    return mat


def ensure_lods(meshes):
    """LODs automaticos para los modelos del kit (vienen con uno solo). Un pino
    de lejos se dibuja con el 20% de sus triangulos; de cerca, completo.
    Con OPTIMIZAR["lods"] = False se vuelve a un solo LOD."""
    sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    niveles = LOD_NIVELES if OPTIMIZAR["lods"] else [(1.0, 1.0)]
    cambiados = 0
    for name, mesh in meshes.items():
        if name in MALLAS_MOTOR or mesh.get_num_lods() == len(niveles):
            continue
        opts = unreal.StaticMeshReductionOptions()
        opts.set_editor_property("auto_compute_lod_screen_size", False)
        ajustes = []
        for pct, pantalla in niveles:
            r = unreal.StaticMeshReductionSettings()
            r.set_editor_property("percent_triangles", pct)
            r.set_editor_property("screen_size", pantalla)
            ajustes.append(r)
        opts.set_editor_property("reduction_settings", ajustes)
        sub.set_lods(mesh, opts)
        eal.save_loaded_asset(mesh)
        cambiados += 1
    ok("LODs: %d niveles por modelo (%d modelos rehechos)" % (len(niveles), cambiados))


def ajustar_altura_pasto(instances, meshes):
    """AlturaRelativa de MI_Pasto y MI_PastoDenso = PASTO_ALTURA / altura media
    de sus matas (medida en las mallas). Asi el pasto mide lo pedido aunque
    se cambien las mallas."""
    def altura_media(nombres):
        hs = [2.0 * meshes[n].get_bounds().box_extent.z / 100.0 for n in nombres if n in meshes]
        return sum(hs) / len(hs) if hs else 0.0

    capa = next((c for c in CAPAS if c["name"] == "Pasto"), None)
    for mi_name, nombres in (("MI_PastoDenso", PASTO_DENSO["mallas"]),
                             ("MI_Pasto", capa["meshes"] if capa else [])):
        h = altura_media(nombres)
        if mi_name not in instances or h <= 0.0:
            continue
        rel = min(1.0, PASTO_ALTURA / h)
        mi = instances[mi_name]
        mel.set_material_instance_scalar_parameter_value(mi, "AlturaRelativa", rel)
        mel.update_material_instance(mi)
        eal.save_loaded_asset(mi)
        ok("%s: matas de %.2f m -> %.2f m (x%.2f)" % (mi_name, h, PASTO_ALTURA, rel))


def build_pasto_procedural(mpc):
    """M_PastoProc: el material de las matas generadas por codigo.

        color  = lerp(Pradera x 0.55, Pasto x 1.15, altura^0.7)   raiz oscura del
                 color del suelo, punta clara del color del pasto: el pasto se
                 funde con el terreno y se lee como un tapete
               x tono propio de cada brizna (+-12%)
               -> tonalidades del bioma (las mismas manchas que el suelo)
        WPO    = viento (peso = altura^2: la raiz no se mueve)
               + desvanecido a lo lejos (desvanecer_wpo)
    El color de vertice lo escribe el C++: R = altura en la brizna, G = azar."""
    mat = recreate_material("M_PastoProc")

    def sparam(nombre, valor, px, py):
        return node(mat, unreal.MaterialExpressionScalarParameter, px, py,
                    parameter_name=nombre, default_value=valor)

    vc = node(mat, unreal.MaterialExpressionVertexColor, -1600, 0)
    h = node(mat, unreal.MaterialExpressionComponentMask, -1400, 0, r=True, g=False, b=False, a=False)
    link(vc, "", h, "")
    azar = node(mat, unreal.MaterialExpressionComponentMask, -1400, 150, r=False, g=True, b=False, a=False)
    link(vc, "", azar, "")

    raiz = node(mat, unreal.MaterialExpressionMultiply, -1100, -300)
    link(mpc_node(mat, mpc, "Pradera", -1400, -350), "", raiz, "A")
    link(sparam("LuzRaiz", 0.55, -1300, -250), "", raiz, "B")
    punta = node(mat, unreal.MaterialExpressionMultiply, -1100, -150)
    link(mpc_node(mat, mpc, "Pasto", -1400, -200), "", punta, "A")
    link(sparam("LuzPunta", 1.15, -1300, -100), "", punta, "B")
    curva = node(mat, unreal.MaterialExpressionPower, -1150, 0, const_exponent=0.7)
    link(h, "", curva, "Base")
    grad = node(mat, unreal.MaterialExpressionLinearInterpolate, -900, -200)
    link(raiz, "", grad, "A"); link(punta, "", grad, "B"); link(curva, "", grad, "Alpha")

    tono = node(mat, unreal.MaterialExpressionLinearInterpolate, -1100, 200, const_a=0.88, const_b=1.12)
    link(azar, "", tono, "Alpha")
    color = node(mat, unreal.MaterialExpressionMultiply, -700, -100)
    link(grad, "", color, "A"); link(tono, "", color, "B")

    usa_pasto = sparam("UsaPasto", 1.0, -900, 400)
    recolorable = sparam("Recolorable", 1.0, -900, 500)
    color = tonalidades_nodes(mat, mpc, color, usa_pasto, recolorable, -600, 600)

    to_prop(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    to_prop(const(mat, 0.85, -300, 300), "", unreal.MaterialProperty.MP_ROUGHNESS)
    to_prop(const(mat, 0.15, -300, 400), "", unreal.MaterialProperty.MP_SPECULAR)

    peso = node(mat, unreal.MaterialExpressionMultiply, -2800, 1200)
    link(h, "", peso, "A"); link(h, "", peso, "B")
    viento = viento_wpo(mat, -2600, 800, dict(altura=1.0, fuerza=PASTO_DENSO["viento"], aleteo=0.0),
                        peso=peso, aleteo=False)
    desvanecer_wpo(mat, viento, -2600, 2200)
    finish(mat)

    mi = recreate_instance("MI_PastoProc", mat)
    r = PASTO_DENSO["radio"] * 100.0
    mel.set_material_instance_scalar_parameter_value(mi, "Desvanecer", 1.0)
    mel.set_material_instance_scalar_parameter_value(mi, "DesvaneceInicio", r * 0.55)
    mel.set_material_instance_scalar_parameter_value(mi, "DesvaneceFin", r)
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi)
    return mi


def agua_desde_color(hexcolor):
    """Color deseado del agua -> coeficientes del Single Layer Water (1/cm).
    Se normaliza el color a su canal mas fuerte (T, de 0 a 1):
        absorcion  = 0.02 + 0.24 (1 - T)   por metro: se come lo que NO es del color
        dispersion = 0.014 T               por metro: un velo del color en lo hondo
        tinte      = 0.65 + 0.40 T         el fondo visto a traves, tenido
    Con el turquesa de AGUA_COLOR da casi los mismos numeros que la v1."""
    c = hex_lin(hexcolor)
    m = max(c) or 1.0
    t = [ch / m for ch in c]
    absorcion = tuple((0.02 + 0.24 * (1.0 - v)) * 0.01 for v in t)
    dispersion = tuple(0.014 * v * 0.01 for v in t)
    tinte = tuple(0.65 + 0.40 * v for v in t)
    return absorcion, dispersion, tinte


def build_palettes():
    out = []
    for p in PALETAS:
        P = unreal.JardinPalette()
        pasto = hex_lin(p["pasto"]) if p["pasto"] else TERRENO_PRADERA
        follaje = hex_lin(p["follaje"]) if p["follaje"] else TERRENO_PRADERA
        bosque = tuple(c * SUELO_BOSQUE for c in follaje) if p["follaje"] else TERRENO_BOSQUE
        P.set_editor_property("name", p["nombre"])
        P.set_editor_property("weight", p["peso"])
        P.set_editor_property("strength", p["fuerza"])
        P.set_editor_property("foliage", lc(follaje))
        P.set_editor_property("grass", lc(pasto))
        P.set_editor_property("meadow", lc(pasto))
        P.set_editor_property("forest", lc(bosque))
        P.set_editor_property("cousin_hue_degrees", float(p.get("primo", 0)))
        if p.get("agua"):
            absorcion, dispersion, tinte = agua_desde_color(p["agua"])
            P.set_editor_property("water_override", True)
            P.set_editor_property("water_absorption", lc(absorcion))
            P.set_editor_property("water_scattering", lc(dispersion))
            P.set_editor_property("water_tint", lc(tinte))
        if p.get("cielo"):
            cielo = hex_lin(p["cielo"])
            P.set_editor_property("sky_scattering", lc(rayleigh(cielo)))
            P.set_editor_property("fog_tint", lc(cielo))
        else:
            P.set_editor_property("sky_scattering", lc(RAYLEIGH_TIERRA))
            P.set_editor_property("fog_tint", lc(NIEBLA_TIERRA))
        out.append(P)
    return out


def configure_garden(meshes, terreno, fractal_mats=None, agua=None, mpc=None, camino=None, instances=None,
                     pasto_proc=None):
    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    gardens = [a for a in sub.get_all_level_actors() if isinstance(a, unreal.JardinWorldActor)]
    if not gardens:
        err("No hay ningun JardinWorldActor en el nivel abierto. Abre L_Fase0 y repite.")
        return None
    g = gardens[0]

    layers = []
    for c in CAPAS:
        L = unreal.JardinPropLayer()
        L.set_editor_property("name", c["name"])

        if "fractal" in c:
            # Sin modelos: el arbol se construye por codigo en cada sitio.
            L.set_editor_property("fractal_tree", True)
            fs = unreal.JardinFractalSettings()
            for k, v in c["fractal"].items():
                fs.set_editor_property(k, v)
            L.set_editor_property("fractal", fs)
            if fractal_mats:
                L.set_editor_property("fractal_leaf_material", fractal_mats["hojas"].get(c.get("hojas", "Rosa")))
                L.set_editor_property("fractal_bark_material", fractal_mats["corteza"].get(c.get("corteza", "Normal")))
            L.set_editor_property("fractal_collision", c.get("colision", False))
        else:
            variants = [meshes[m] for m in c["meshes"] if m in meshes]
            if not variants:
                err("Capa %s sin ningun modelo disponible" % c["name"])
                continue
            L.set_editor_property("mesh", variants[0])
            L.set_editor_property("variants", variants)
        L.set_editor_property("allowed_zones", c["zones"])
        L.set_editor_property("spacing_meters", c["spacing"])
        L.set_editor_property("exclusion_radius_meters", c["radius"])
        L.set_editor_property("density_frequency", c["dfreq"])
        L.set_editor_property("density_threshold", c["dthr"])
        L.set_editor_property("max_slope", c["slope"])
        L.set_editor_property("min_height01", c["hmin"])
        L.set_editor_property("max_height01", c["hmax"])
        L.set_editor_property("bounds_margin_meters", 2.0)
        smin, smax = c["smin"], c["smax"]
        if "diametro" in c and c["meshes"] and c["meshes"][0] in meshes:
            # Escala = diametro deseado / diametro real de la malla.
            d = mesh_diameter_m(meshes[c["meshes"][0]])
            if d > 0:
                smin, smax = c["diametro"][0] / d, c["diametro"][1] / d
        L.set_editor_property("min_scale", smin)
        L.set_editor_property("max_scale", smax)
        L.set_editor_property("max_count", c["maxc"])

        moist = c.get("moist", (0.0, 1.0))
        L.set_editor_property("min_moisture", moist[0])
        L.set_editor_property("max_moisture", moist[1])
        L.set_editor_property("clustering", c.get("cluster", 0.0))
        L.set_editor_property("is_canopy", c.get("canopy", False))
        L.set_editor_property("require_canopy", "near" in c)
        L.set_editor_property("canopy_distance_meters", c.get("near", 5.0))
        L.set_editor_property("avoid_canopy", "avoid" in c)
        L.set_editor_property("clearing_distance_meters", c.get("avoid", 4.0))
        L.set_editor_property("min_water_distance_meters", c.get("agua", 1.5))
        L.set_editor_property("night_only", c.get("nocturna", False))
        L.set_editor_property("visible", not (c["name"] == "Pasto" and PASTO_DENSO["activo"]))
        L.set_editor_property("min_path_distance_meters", c.get("camino", 0.5))
        L.set_editor_property("cull_distance_meters",
                              float(CORTE.get(c["name"], 0)) if OPTIMIZAR["cull"] else 0.0)
        layers.append(L)

    s = g.get_editor_property("settings")
    for k, v in RELIEVE.items():
        s.set_editor_property(k, v)
    s.set_editor_property("water_mode", getattr(unreal.JardinWaterMode, AGUA))
    for k, v in CAMINO.items():
        s.set_editor_property(k, v)
    g.set_editor_property("settings", s)

    g.set_editor_property("prop_layers", layers)
    g.set_editor_property("random_seed_on_begin_play", SEMILLA_AL_AZAR)
    if terreno:
        g.set_editor_property("terrain_material", terreno)
    if agua:
        g.set_editor_property("water_material", agua)
    if camino:
        g.set_editor_property("path_material", camino)

    D = PASTO_DENSO
    pasto = [meshes[m] for m in D["mallas"] if m in meshes]
    g.set_editor_property("dense_grass", bool(D["activo"] and (pasto or D["procedural"])))
    g.set_editor_property("procedural_grass", D["procedural"])
    g.set_editor_property("grass_blades", D["briznas"])
    g.set_editor_property("grass_blade_height_meters", D["altura_brizna"])
    g.set_editor_property("grass_clump_radius_meters", D["radio_mata"])
    g.set_editor_property("grass_blade_width_meters", D["ancho_brizna"])
    g.set_editor_property("grass_clump_variants", D["variantes"])
    g.set_editor_property("grass_meshes", pasto)
    if D["procedural"] and pasto_proc:
        g.set_editor_property("grass_material", pasto_proc)
    elif instances and "MI_PastoDenso" in instances:
        g.set_editor_property("grass_material", instances["MI_PastoDenso"])
    g.set_editor_property("grass_radius_meters", D["radio"])
    g.set_editor_property("grass_chunk_meters", D["trozo"])
    g.set_editor_property("grass_density", D["densidad"])
    g.set_editor_property("grass_forest_factor", D["bosque"])
    g.set_editor_property("grass_patch_cut", D["corte_mancha"])
    g.set_editor_property("grass_min_scale", D["escala"][0])
    g.set_editor_property("grass_max_scale", D["escala"][1])
    g.set_editor_property("grass_chunks_per_frame", D["trozos_por_frame"])
    g.set_editor_property("palettes", build_palettes())
    g.set_editor_property("forced_palette", PALETA_FIJA)
    if mpc:
        g.set_editor_property("biome_collection", mpc)
    g.generate()
    ok("JardinWorldActor: %d capas con ecologia + relieve con crestas + %d biomas"
       % (len(layers), len(PALETAS)))
    return g


def report(g):
    counts = list(g.get_prop_counts())
    layers = list(g.get_editor_property("prop_layers"))
    unreal.log("[Jardin] ------------ objetos por capa ------------")
    total_tris = [0.0]
    total = 0
    for L, n in zip(layers, counts):
        name = str(L.get_editor_property("name"))
        cap = L.get_editor_property("max_count")
        if L.get_editor_property("fractal_tree"):
            kind = "fractales: todos distintos"
        else:
            kind = "%d variantes" % len(L.get_editor_property("variants"))
        flag = "   <-- llego al tope: sube max_count o el reparto queda cortado" if n >= cap else ""
        tris = ""
        variants = list(L.get_editor_property("variants")) if not L.get_editor_property("fractal_tree") else []
        if variants and n:
            media = sum(v.get_num_triangles(0) for v in variants) / float(len(variants))
            total_tris[0] += media * n
            tris = "  %6.1f M triangulos (%d c/u, %d LOD)" % (media * n / 1e6, media, variants[0].get_num_lods())
        cull = L.get_editor_property("cull_distance_meters")
        tris += ("  corte %d m" % cull) if cull > 0 else ""
        unreal.log("[Jardin]   %-16s %7d  (%s)%s%s" % (name, n, kind, tris, flag))
        if n >= cap:
            warn("La capa %s llego a su tope de %d" % (name, cap))
        total += n
    unreal.log("[Jardin]   %-16s %7d  %6.1f M triangulos en LOD0 (sin contar fractales)"
               % ("TOTAL", total, total_tris[0] / 1e6))
    unreal.log("[Jardin]   relieve: %s | bioma: %s | camino: %.0f m"
               % (g.get_relief_name(), g.get_palette_name(), g.get_path_length()))
    unreal.log("[Jardin]   agua: %s, %.0f%% del mapa" % (g.get_water_profile_name(), g.get_water_percent()))
    unreal.log("[Jardin]   generado en %.1f ms" % g.get_last_generation_ms())


# ======================================================================
#  material de los arboles fractales
# ======================================================================

# Colores lineales. El canal G del color de vertice trae un numero aleatorio
# por arbol (derivado de su semilla), y el material mezcla entre A y B con el:
# cada arbol rosa tiene su propio matiz.
FRACTAL_HOJAS = {
    "Rosa":    dict(a=(0.95, 0.30, 0.55), b=(1.00, 0.55, 0.78), brillo=0.08),
    "Rojizo":  dict(a=(0.80, 0.08, 0.20), b=(1.00, 0.28, 0.40), brillo=0.08),
    # Magenta intenso, como el arbol gigante de la referencia. Brilla un poco
    # mas: de noche se ve a lo lejos entre la niebla, como un faro.
    "Colosal": dict(a=(0.90, 0.12, 0.42), b=(1.00, 0.40, 0.70), brillo=0.14),
    "ColosalCarmesi": dict(a=(0.75, 0.04, 0.12), b=(0.98, 0.20, 0.28), brillo=0.14),
}
FRACTAL_CORTEZA = {
    "Normal":  dict(a=(0.22, 0.13, 0.09), b=(0.34, 0.21, 0.14), brillo=0.0),
    "Colosal": dict(a=(0.16, 0.11, 0.08), b=(0.26, 0.19, 0.14), brillo=0.0),   # madera vieja, mas oscura
}


def build_fractal_materials():
    """Las mallas fractales se construyen por codigo y no traen texturas: el
    color sale de dos parametros y del tono del ejemplar.

    'Brillo' es una emision muy tenue: de dia apenas se nota, pero de noche
    hace que los arboles rosas se distingan entre la niebla."""
    mat = recreate_material("M_Fractal")
    mat.set_editor_property("two_sided", True)

    vc = node(mat, unreal.MaterialExpressionVertexColor, -900, 0)
    a = vparam(mat, "ColorA", FRACTAL_HOJAS["Rosa"]["a"], -900, 250)
    b = vparam(mat, "ColorB", FRACTAL_HOJAS["Rosa"]["b"], -900, 400)
    tono = node(mat, unreal.MaterialExpressionLinearInterpolate, -600, 300)
    link(a, "", tono, "A"); link(b, "", tono, "B"); link(vc, "G", tono, "Alpha")

    brillo = node(mat, unreal.MaterialExpressionScalarParameter, -600, 550,
                  parameter_name="Brillo", default_value=0.0)
    emis = node(mat, unreal.MaterialExpressionMultiply, -300, 450)
    link(tono, "", emis, "A"); link(brillo, "", emis, "B")

    to_prop(tono, "", unreal.MaterialProperty.MP_BASE_COLOR)
    to_prop(emis, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    to_prop(const(mat, 0.80, -300, 650), "", unreal.MaterialProperty.MP_ROUGHNESS)
    # El C++ guarda en cada vertice cuanto se dobla (R: 0 base, 1 hojas) y un
    # numero al azar por arbol (G): son el peso y la fase del viento. Corteza
    # y hojas comparten material, asi que se mueven juntas.
    vcw = node(mat, unreal.MaterialExpressionVertexColor, -2600, 1200)
    flex = node(mat, unreal.MaterialExpressionComponentMask, -2400, 1200, r=True, g=False, b=False, a=False)
    link(vcw, "", flex, "")
    flex2 = node(mat, unreal.MaterialExpressionMultiply, -2250, 1200)
    link(flex, "", flex2, "A"); link(flex, "", flex2, "B")
    fase_arbol = node(mat, unreal.MaterialExpressionComponentMask, -2400, 1300, r=False, g=True, b=False, a=False)
    link(vcw, "", fase_arbol, "")
    viento_wpo(mat, -2600, 800, dict(altura=1.0, **VIENTO_FRACTAL), peso=flex2, fase=fase_arbol, aleteo=True)
    finish(mat)

    out = {"hojas": {}, "corteza": {}}
    for parte, paletas in (("hojas", FRACTAL_HOJAS), ("corteza", FRACTAL_CORTEZA)):
        for nombre, cfg in paletas.items():
            mi = recreate_instance("MI_Fractal_%s_%s" % (parte.capitalize(), nombre), mat)
            mel.set_material_instance_vector_parameter_value(mi, "ColorA", lc(cfg["a"]))
            mel.set_material_instance_vector_parameter_value(mi, "ColorB", lc(cfg["b"]))
            mel.set_material_instance_scalar_parameter_value(mi, "Brillo", cfg["brillo"])
            mel.update_material_instance(mi)
            eal.save_loaded_asset(mi)
            out[parte][nombre] = mi
    ok("M_Fractal + %d paletas de hojas + %d de corteza" % (len(FRACTAL_HOJAS), len(FRACTAL_CORTEZA)))
    return out


# ======================================================================
#  luciernagas
# ======================================================================

# Color lineal y brillo de la luz. De noche la exposicion esta fija (0.5 EV,
# atmosfera.py): con brillo ~40 el bloom las convierte en puntos de luz.
LUCIERNAGA = dict(
    color=(0.70, 1.00, 0.20),   # amarillo verdoso, como las de verdad
    brillo=40.0,
    radio=180.0,       # cm que se alejan de su punto de anclaje al vagar
    altura_min=50.0,   # cm sobre el suelo
    altura_var=230.0,  # cada una vuela a su altura: de 0.5 a 2.8 m
    ritmo=0.30,        # destellos por segundo (cada una con su variacion)
)

# Vuelo (por instancia, R = numero al azar del ejemplar, T = tiempo):
#   a = 2pi R,  b = 2pi frac(7.31 R),  c = frac(13.17 R)
#   x = Radio (sin(0.31 T + a) + 0.35 sin(0.83 T + 2b))
#   y = Radio (cos(0.27 T + b) + 0.35 cos(0.71 T + 3a))
#   z = AlturaMin + c AlturaVar + 35 sin(0.53 T + 5a)
# Frecuencias que no son multiplos entre si: la trayectoria no se repite a
# simple vista. Fase propia por ejemplar: no hay dos que vuelen igual.
#
# Destello: ciclo = frac(T Ritmo (0.7 + 0.6 frac(5.3 R)) + 9 frac(3.71 R))
#   enciende en el 6% del ciclo, se apaga hasta el 35%, y el resto casi
#   apagada (0.015), como las luciernagas reales.
#
# v1 hacia esto en nodos Custom (HLSL) y M_Luciernaga dejo de compilar: el
# nodo Custom no recibe el numero por instancia ("missing input R"). Con
# nodos estandar, como el viento, compila siempre.


def mesh_diameter_m(mesh):
    """Diametro de la esfera envolvente de la malla, en metros."""
    return 2.0 * mesh.get_bounds().sphere_radius / 100.0


def build_luciernaga(mesh):
    mat = recreate_material("M_Luciernaga")
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)

    def sparam(nombre, valor, px, py):
        return node(mat, unreal.MaterialExpressionScalarParameter, px, py,
                    parameter_name=nombre, default_value=valor)

    def op(cls, px, py, a=None, b=None, **k):
        n = node(mat, cls, px, py, **k)
        if a is not None:
            link(a, "", n, "A")
        if b is not None:
            link(b, "", n, "B")
        return n

    def mul(a, b=None, px=0, py=0, k=None):
        return op(unreal.MaterialExpressionMultiply, px, py, a, b, **({"const_b": k} if k is not None else {}))

    def add(a, b=None, px=0, py=0, k=None):
        return op(unreal.MaterialExpressionAdd, px, py, a, b, **({"const_b": k} if k is not None else {}))

    def frac(a, px, py):
        n = node(mat, unreal.MaterialExpressionFrac, px, py)
        link(a, "", n, "")
        return n

    def onda(fase_t, fase, px, py, desfase=0.0):
        """sin(fase_t * T + fase + desfase); el coseno es el seno desfasado pi/2."""
        arg = add(mul(tm, None, px, py, fase_t), fase, px + 150, py)
        if desfase:
            arg = add(arg, None, px + 300, py, desfase)
        sn = node(mat, unreal.MaterialExpressionSine, px + 450, py, period=6.2831853)
        link(arg, "", sn, "")
        return sn

    L = LUCIERNAGA
    tm = node(mat, unreal.MaterialExpressionTime, -2400, 0)
    rnd = node(mat, unreal.MaterialExpressionPerInstanceRandom, -2400, 100)
    radio = sparam("Radio", L["radio"], -2400, 250)

    # --- vuelo
    fa = mul(rnd, None, -2200, 200, 6.2831853)
    fb = mul(frac(mul(rnd, None, -2200, 300, 7.31), -2050, 300), None, -1900, 300, 6.2831853)
    fc = frac(mul(rnd, None, -2200, 400, 13.17), -2050, 400)
    hpi = 1.5707963

    x = add(onda(0.31, fa, -1800, 100), mul(onda(0.83, mul(fb, None, -1950, 150, 2.0), -1800, 200), None, -1200, 200, 0.35), -1050, 150)
    y = add(onda(0.27, fb, -1800, 350, hpi), mul(onda(0.71, mul(fa, None, -1950, 450, 3.0), -1800, 450, hpi), None, -1200, 450, 0.35), -1050, 400)
    x = mul(x, radio, -900, 150)
    y = mul(y, radio, -900, 400)
    z = add(add(sparam("AlturaMin", L["altura_min"], -1300, 650), mul(fc, sparam("AlturaVar", L["altura_var"], -1300, 750), -1150, 700), -1000, 650),
            mul(onda(0.53, mul(fa, None, -1950, 850, 5.0), -1800, 850), None, -1150, 850, 35.0), -850, 700)
    xy = node(mat, unreal.MaterialExpressionAppendVector, -700, 250)
    link(x, "", xy, "A"); link(y, "", xy, "B")
    vuelo = node(mat, unreal.MaterialExpressionAppendVector, -550, 400)
    link(xy, "", vuelo, "A"); link(z, "", vuelo, "B")

    # --- destello
    ritmo = mul(sparam("Ritmo", L["ritmo"], -2400, -300),
                add(mul(frac(mul(rnd, None, -2200, -200, 5.3), -2050, -200), None, -1900, -200, 0.6), None, -1750, -200, 0.7),
                -1600, -250)
    ciclo = frac(add(mul(tm, ritmo, -1450, -300), mul(frac(mul(rnd, None, -2200, -100, 3.71), -2050, -100), None, -1900, -100, 9.0), -1300, -250), -1150, -250)
    sube = node(mat, unreal.MaterialExpressionSaturate, -950, -350)
    link(mul(ciclo, None, -1050, -350, 1.0 / 0.06), "", sube, "")
    baja0 = node(mat, unreal.MaterialExpressionSaturate, -950, -200)
    link(mul(add(ciclo, None, -1100, -200, -0.06), None, -1050, -200, 1.0 / 0.29), "", baja0, "")
    baja = node(mat, unreal.MaterialExpressionOneMinus, -800, -200)
    link(baja0, "", baja, "")
    pulso = mul(sube, baja, -650, -300)
    destello = add(mul(pulso, pulso, -500, -300), None, -350, -300, 0.015)   # al cuadrado: encendido mas suave

    color = vparam(mat, "Color", L["color"], -600, -400)
    c1 = node(mat, unreal.MaterialExpressionMultiply, -300, -300)
    link(color, "", c1, "A"); link(destello, "", c1, "B")
    # Brillo suave: maximo en el centro de la esfera y se apaga hacia el
    # contorno. Asi se ve una bolita de luz y no una esfera solida.
    fres = node(mat, unreal.MaterialExpressionFresnel, -600, -650, exponent=1.5)
    halo = node(mat, unreal.MaterialExpressionOneMinus, -450, -650)
    link(fres, "", halo, "")
    halo2 = node(mat, unreal.MaterialExpressionMultiply, -300, -600)
    link(halo, "", halo2, "A"); link(halo, "", halo2, "B")
    c2 = node(mat, unreal.MaterialExpressionMultiply, -150, -450)
    link(c1, "", c2, "A"); link(halo2, "", c2, "B")
    emis = node(mat, unreal.MaterialExpressionMultiply, -150, -300)
    link(c2, "", emis, "A"); link(sparam("Brillo", L["brillo"], -300, -150), "", emis, "B")

    to_prop(emis, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    to_prop(vuelo, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    finish(mat)

    mesh.set_material(0, mat)
    eal.save_loaded_asset(mesh)
    return mat


# ======================================================================
#  main
# ======================================================================

def jugando():
    """True si hay una partida (Play) en marcha. Durante Play, Unreal desactiva
    las funciones de assets de Python: el script creeria que no existe nada,
    intentaria reimportar los 53 modelos y recrear MPC_Bioma (paso el 7-oct)."""
    try:
        return unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
    except Exception:
        return False


def main():
    if jugando():
        unreal.log_error("[Jardin] El juego esta corriendo (Play). Detenlo con Stop y vuelve a ejecutar el script.")
        return
    for f in (F_TEX, F_MESH, F_MAT):
        eal.make_directory(f)

    if not os.path.isdir(KIT):
        err("No encuentro el kit en " + KIT)
        return

    tex = ensure_textures()
    meshes = ensure_meshes()
    try:
        ensure_lods(meshes)
    except Exception as e:
        err("LODs: %s" % e)

    mpc = None
    try:
        mpc = ensure_mpc()
        ok("MPC_Bioma (%d paletas de color)" % len(PALETAS))
    except Exception as e:
        err("MPC_Bioma: %s (la vegetacion queda con sus colores originales)" % e)

    instances = {}
    try:
        instances = build_instances(tex, mpc)
    except Exception as e:
        err("Materiales: %s" % e)

    try:
        ajustar_altura_pasto(instances, meshes)
    except Exception as e:
        err("Altura del pasto: %s" % e)

    terreno = None
    try:
        terreno = build_terreno(mpc)
        ok("M_Terreno")
    except Exception as e:
        err("Terreno: %s" % e)

    try:
        assign_materials(meshes, instances)
    except Exception as e:
        err("Asignar materiales: %s" % e)

    for viejo in MALLAS_OBSOLETAS:
        if eal.does_asset_exist(viejo):
            eal.delete_asset(viejo)
    if "SM_LuciernagaEsfera" in meshes:
        try:
            build_luciernaga(meshes["SM_LuciernagaEsfera"])
            ok("M_Luciernaga (vuelo y destello en la GPU)")
        except Exception as e:
            err("Luciernagas: %s" % e)

    fractal_mats = None
    try:
        fractal_mats = build_fractal_materials()
    except Exception as e:
        err("Material fractal: %s" % e)

    agua = None
    try:
        agua = build_agua(mpc)
        ok("M_Agua (Single Layer Water)")
    except Exception as e:
        err("Agua: %s" % e)

    pasto_proc = None
    if PASTO_DENSO["procedural"] and mpc:
        try:
            pasto_proc = build_pasto_procedural(mpc)
            ok("M_PastoProc (pasto procedural)")
        except Exception as e:
            err("Pasto procedural: %s" % e)

    camino = None
    try:
        camino = build_camino()
        ok("M_Camino")
    except Exception as e:
        err("Camino: %s" % e)

    g = None
    try:
        g = configure_garden(meshes, terreno, fractal_mats, agua, mpc, camino, instances, pasto_proc)
    except Exception as e:
        err("Configurar jardin: %s" % e)

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)

    if g:
        try:
            report(g)
        except Exception as e:
            warn("No se pudo leer el reporte: %s" % e)

    unreal.log("[Jardin] ================= RESUMEN =================")
    for m in log_ok:
        unreal.log("[Jardin]   OK     " + m)
    for m in log_warn:
        unreal.log_warning("[Jardin]   AVISO  " + m)
    for m in log_err:
        unreal.log_error("[Jardin]   ERROR  " + m)
    unreal.log("[Jardin] %d correctos, %d avisos, %d errores" % (len(log_ok), len(log_warn), len(log_err)))


main()
