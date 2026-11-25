# -*- coding: utf-8 -*-
"""
atmosfera.py — Fase 1: luz, niebla y post-proceso del jardin.

Ejecutar en el editor:  Tools -> Execute Python Script... -> este archivo

Configura las luces que ya trae el nivel (sol, cielo, atmosfera, niebla) y crea
un volumen de post-proceso. Es repetible: cambia AMBIENTE y vuelve a ejecutarlo.

Que aporta cada pieza a la "vividez":
  Sol bajo y calido      sombras largas que dibujan el relieve
  Niebla de altura       valles brumosos, cimas despejadas: profundidad
  Perspectiva aerea      las montanas lejanas se azulan, como en la realidad
  Niebla volumetrica     rayos de luz entre los arboles
  Post-proceso           saturacion, contraste, brillo suave, vineta

Relacion con el temario: modelo de iluminacion (luz direccional + ambiental),
modelos de color (temperatura de color en Kelvin, saturacion, contraste) y
transparencia/atenuacion (la niebla es atenuacion exponencial con la distancia).
"""

import unreal

# =====================================================================
#  ELIGE EL AMBIENTE:  "tarde"  |  "mediodia"  |  "amanecer"  |  "noche"
# =====================================================================

AMBIENTE = "mediodia"

# =====================================================================
#  NIEBLA ESPESA (estilo Silent Hill, pero transitable). Funciona con
#  cualquier ambiente.
# =====================================================================

# "auto" = solo de noche.  True = siempre (una manana brumosa tambien queda bien).  False = nunca.
NIEBLA_ESPESA = "auto"

# Unreal atenua la luz de forma exponencial: transmitancia = exp(-k * d), con
# k = densidad / 1000 por unidad (cm). La visibilidad cae a la MITAD en
#     d = ln(2) / k   ->   densidad 0.15  =>  ~46 m
#                          densidad 0.25  =>  ~28 m   (Silent Hill de verdad)
#                          densidad 0.08  =>  ~87 m   (bruma ligera)
# La caida con la altura casi nula hace que la niebla envuelva tambien las
# colinas, en vez de quedarse en el fondo de los valles.
NIEBLA_ESPESA_PARAMS = dict(
    densidad=0.15,
    caida=0.05,
    inicio=800.0,          # 8 m alrededor de la camara se ven nitidos
    opacidad_max=0.97,     # a lo lejos el mundo desaparece casi por completo
    extincion_volumetrica=1.6,
    distancia_volumetrica=6000.0,   # mas alla de 60 m ya es opaca: no gastar en ella
    aclarar_color=1.5,     # la niebla espesa debe verse; si no, es un muro negro de noche
)

# Version 2. Primera version: mirar hacia el sol dejaba un velo blanco. Lo
# causaba la DISPERSION HACIA ADELANTE (volumetrica_dispersion alta, niebla_sol
# fuerte): el aire entre la camara y el sol se iluminaba. Mas el bloom encima.
# Se bajaron esos valores y la densidad de la niebla en todos los ambientes.
#
#   rayos_sol      fuerza de los rayos de luz en pantalla (0 = apagados)
#   ganancia       tinte final de la imagen (1, 1, 1 = neutro)
#   exposicion     None = automatica; un numero = fija, en EV100. Sin fijarla,
#                  la camara se adapta a la oscuridad y la noche parece de dia.

AMBIENTES = {
    # Hora dorada: el sol bajo realza el relieve con sombras largas.
    "tarde": dict(
        sol_altura=22.0,  sol_rumbo=150.0,  sol_kelvin=4600.0, sol_intensidad=9.0,
        niebla_densidad=0.016, niebla_caida=0.15, niebla_inicio=3500.0,
        niebla_color=(0.40, 0.55, 0.85), niebla_sol=(0.12, 0.08, 0.04), niebla_sol_exp=8.0,
        volumetrica=True, volumetrica_dispersion=0.35,
        perspectiva_aerea=2.5, bruma=0.0045, rayos_sol=0.10,
        saturacion=1.10, contraste=1.06, bloom=0.40, vineta=0.30, oclusion=0.70,
        cielo_intensidad=1.0, cielo_color=(255, 255, 255),
        ganancia=(1.0, 1.0, 1.0), exposicion=None,
    ),
    # Luz neutra y alta: colores mas fieles, menos dramatismo.
    "mediodia": dict(
        sol_altura=55.0,  sol_rumbo=120.0,  sol_kelvin=5800.0, sol_intensidad=10.0,
        niebla_densidad=0.010, niebla_caida=0.20, niebla_inicio=5000.0,
        niebla_color=(0.45, 0.62, 0.95), niebla_sol=(0.05, 0.04, 0.03), niebla_sol_exp=8.0,
        volumetrica=True, volumetrica_dispersion=0.30,
        perspectiva_aerea=2.0, bruma=0.0035, rayos_sol=0.06,
        saturacion=1.08, contraste=1.04, bloom=0.35, vineta=0.25, oclusion=0.60,
        cielo_intensidad=1.0, cielo_color=(255, 255, 255),
        ganancia=(1.0, 1.0, 1.0), exposicion=None,
    ),
    # Sol rasante y bruma en los valles.
    "amanecer": dict(
        sol_altura=9.0,   sol_rumbo=80.0,   sol_kelvin=3600.0, sol_intensidad=6.0,
        niebla_densidad=0.028, niebla_caida=0.25, niebla_inicio=2000.0,
        niebla_color=(0.55, 0.55, 0.72), niebla_sol=(0.25, 0.15, 0.07), niebla_sol_exp=8.0,
        volumetrica=True, volumetrica_dispersion=0.45,
        perspectiva_aerea=3.0, bruma=0.0060, rayos_sol=0.12,
        saturacion=1.12, contraste=1.08, bloom=0.50, vineta=0.35, oclusion=0.75,
        cielo_intensidad=1.0, cielo_color=(255, 255, 255),
        ganancia=(1.0, 1.0, 1.0), exposicion=None,
    ),
    # Noche en azul profundo. La "luna" es la misma luz direccional: alta,
    # tenue y muy fria (12000 K). Una noche fisicamente correcta seria casi
    # negra; esta es estilizada, como en los juegos: oscura pero legible.
    "noche": dict(
        sol_altura=35.0,  sol_rumbo=210.0,  sol_kelvin=12000.0, sol_intensidad=1.2,
        niebla_densidad=0.020, niebla_caida=0.20, niebla_inicio=2000.0,
        niebla_color=(0.04, 0.07, 0.18), niebla_sol=(0.02, 0.03, 0.06), niebla_sol_exp=8.0,
        volumetrica=True, volumetrica_dispersion=0.30,
        perspectiva_aerea=2.0, bruma=0.0040, rayos_sol=0.0,
        saturacion=1.10, contraste=1.10, bloom=0.55, vineta=0.50, oclusion=0.70,
        cielo_intensidad=0.6, cielo_color=(150, 180, 255),
        # v2: menos verde (el pasto tiraba a turquesa).
        ganancia=(0.72, 0.78, 1.15), exposicion=0.5,
        # v2: de noche el cielo debe ser MAS OSCURO que el suelo iluminado por
        # la luna; antes era al reves y parecia "dia con filtro azul".
        cielo_luminancia=(0.30, 0.35, 0.50),
        # v2: la luna da sombras suaves. Un angulo aparente mayor las difumina.
        fuente_angulo=3.0,
    ),
}

# =====================================================================

aplicados, fallidos = [], []


def niebla_espesa_activa():
    if NIEBLA_ESPESA == "auto":
        return AMBIENTE == "noche"
    return bool(NIEBLA_ESPESA)


def setp(obj, prop, value, label):
    """Asigna una propiedad sin detener el script si su nombre cambio en esta version."""
    if obj is None:
        fallidos.append("%s.%s (no existe el objeto)" % (label, prop))
        return False
    try:
        obj.set_editor_property(prop, value)
        aplicados.append("%s.%s" % (label, prop))
        return True
    except Exception as e:
        fallidos.append("%s.%s (%s)" % (label, prop, str(e).split("\n")[0][:90]))
        return False


def setp_any(obj, props, value, label):
    """Prueba varios nombres: algunas propiedades se renombraron entre versiones de UE5."""
    for p in props:
        try:
            obj.set_editor_property(p, value)
            aplicados.append("%s.%s" % (label, p))
            return True
        except Exception:
            continue
    fallidos.append("%s.%s (ningun nombre valido)" % (label, "/".join(props)))
    return False


def lc(rgb):
    return unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0)


def main():
    # Durante Play, Unreal desactiva las funciones de editor de Python.
    try:
        if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
            unreal.log_error("[Atmosfera] El juego esta corriendo (Play). Detenlo con Stop y vuelve a ejecutar el script.")
            return
    except Exception:
        pass
    if AMBIENTE not in AMBIENTES:
        unreal.log_error("[Atmosfera] Ambiente desconocido: %s" % AMBIENTE)
        return
    A = AMBIENTES[AMBIENTE]

    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actores = sub.get_all_level_actors()

    def primero(cls):
        return next((a for a in actores if isinstance(a, cls)), None)

    # ------------------------------------------------------------ sol
    sol = primero(unreal.DirectionalLight)
    if sol:
        # Altura del sol = inclinacion hacia abajo de la luz.
        sol.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=-A["sol_altura"], yaw=A["sol_rumbo"]), False)
        c = sol.get_component_by_class(unreal.DirectionalLightComponent)
        setp(c, "intensity", A["sol_intensidad"], "Sol")
        # Temperatura de color en Kelvin: menos K = luz mas calida.
        setp(c, "use_temperature", True, "Sol")
        setp(c, "temperature", A["sol_kelvin"], "Sol")
        setp(c, "atmosphere_sun_light", True, "Sol")
        # Tamano aparente de la fuente de luz, en grados. El sol mide ~0.53;
        # mas grande = penumbra mas ancha = sombras mas suaves.
        setp(c, "light_source_angle", A.get("fuente_angulo", 0.5357), "Sol")
        setp(c, "cast_cloud_shadows", True, "Sol")
        # Sombras de contacto: asientan pasto y rocas en el suelo.
        setp(c, "contact_shadow_length", 0.04, "Sol")
        # Rayos de luz en pantalla cuando el sol queda tras los arboles.
        setp(c, "enable_light_shaft_occlusion", A["rayos_sol"] > 0.0, "Sol")
        setp(c, "enable_light_shaft_bloom", A["rayos_sol"] > 0.0, "Sol")
        setp(c, "bloom_scale", A["rayos_sol"], "Sol")
        setp(c, "bloom_threshold", 0.3, "Sol")
    else:
        fallidos.append("No hay DirectionalLight en el nivel")

    # ------------------------------------------------------------ cielo
    cielo = primero(unreal.SkyLight)
    if cielo:
        c = cielo.get_component_by_class(unreal.SkyLightComponent)
        # Captura en tiempo real: la luz ambiental sigue al sol y a la niebla.
        setp(c, "real_time_capture", True, "Cielo")
        setp(c, "intensity", A["cielo_intensidad"], "Cielo")
        r, g, b = A["cielo_color"]
        setp(c, "light_color", unreal.Color(r=r, g=g, b=b, a=255), "Cielo")
        try:
            c.recapture_sky()
        except Exception:
            pass

    atm = primero(unreal.SkyAtmosphere)
    if atm:
        c = atm.get_component_by_class(unreal.SkyAtmosphereComponent)
        # El mapa mide 1 km: en la vida real la perspectiva aerea se nota a
        # decenas de km. Se exagera la escala para que las cordilleras del fondo
        # se azulen como montanas lejanas.
        setp_any(c, ["aerial_pespective_view_distance_scale", "aerial_perspective_view_distance_scale"],
                 A["perspectiva_aerea"], "Atmosfera")
        setp(c, "mie_scattering_scale", A["bruma"], "Atmosfera")
        # Brillo del cielo, independiente del de la escena. Neutro de dia.
        setp(c, "sky_luminance_factor", lc(A.get("cielo_luminancia", (1.0, 1.0, 1.0))), "Atmosfera")

    # ------------------------------------------------------------ niebla
    niebla = primero(unreal.ExponentialHeightFog)
    if niebla:
        # La niebla se mide desde la altura del actor: en el fondo de los valles.
        loc = niebla.get_actor_location()
        niebla.set_actor_location(unreal.Vector(loc.x, loc.y, 0.0), False, False)
        c = niebla.get_component_by_class(unreal.ExponentialHeightFogComponent)

        densidad, caida, inicio = A["niebla_densidad"], A["niebla_caida"], A["niebla_inicio"]
        opacidad, extincion, dist_vol = 0.9, 1.0, 8000.0
        color = A["niebla_color"]
        if niebla_espesa_activa():
            E = NIEBLA_ESPESA_PARAMS
            densidad, caida, inicio = E["densidad"], E["caida"], E["inicio"]
            opacidad, extincion, dist_vol = E["opacidad_max"], E["extincion_volumetrica"], E["distancia_volumetrica"]
            color = tuple(min(1.0, ch * E["aclarar_color"]) for ch in color)

        setp(c, "fog_density", densidad, "Niebla")
        # Caida con la altura: alta = bruma solo en los valles; baja = lo envuelve todo.
        setp(c, "fog_height_falloff", caida, "Niebla")
        setp(c, "start_distance", inicio, "Niebla")
        setp(c, "fog_max_opacity", opacidad, "Niebla")
        setp_any(c, ["fog_inscattering_luminance", "fog_inscattering_color"], lc(color), "Niebla")
        # Resplandor calido de la niebla en la direccion del sol.
        setp_any(c, ["directional_inscattering_luminance", "directional_inscattering_color"],
                 lc(A["niebla_sol"]), "Niebla")
        setp(c, "directional_inscattering_exponent", A["niebla_sol_exp"], "Niebla")
        setp(c, "directional_inscattering_start_distance", 5000.0, "Niebla")
        # Niebla volumetrica: la luz se dispersa en el aire y aparecen rayos
        # entre los troncos. Es la pieza que mas "vida" da a un bosque.
        setp(c, "enable_volumetric_fog", A["volumetrica"], "Niebla")
        setp(c, "volumetric_fog_scattering_distribution", A["volumetrica_dispersion"], "Niebla")
        setp(c, "volumetric_fog_extinction_scale", extincion, "Niebla")
        setp(c, "volumetric_fog_distance", dist_vol, "Niebla")
    else:
        fallidos.append("No hay ExponentialHeightFog en el nivel")

    # ------------------------------------------------------------ post-proceso
    ppv = next((a for a in actores if isinstance(a, unreal.PostProcessVolume)
                and a.get_actor_label() == "PP_Jardin"), None)
    if ppv is None:
        ppv = sub.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
        ppv.set_actor_label("PP_Jardin")
    setp(ppv, "unbound", True, "PostProceso")   # afecta a todo el mapa

    s = ppv.get_editor_property("settings")
    v4 = lambda k: unreal.Vector4(k, k, k, 1.0)
    for prop, value in (
        ("bloom_intensity", A["bloom"]),
        ("color_saturation", v4(A["saturacion"])),
        ("color_contrast", v4(A["contraste"])),
        ("vignette_intensity", A["vineta"]),
        ("ambient_occlusion_intensity", A["oclusion"]),
        # Exposicion local: recupera detalle en las sombras del bosque sin
        # quemar el cielo.
        ("local_exposure_shadow_contrast_scale", 0.8),
        ("local_exposure_highlight_contrast_scale", 0.85),
        # Tinte final: neutro de dia, azul de noche.
        ("color_gain", unreal.Vector4(A["ganancia"][0], A["ganancia"][1], A["ganancia"][2], 1.0)),
    ):
        # Cada ajuste del post-proceso tiene su casilla "override" que lo activa.
        if setp(s, "override_" + prop, True, "PostProceso"):
            setp(s, prop, value, "PostProceso")

    # Exposicion: fija de noche, automatica de dia. Hay que DESACTIVAR la fija
    # al volver a un ambiente de dia; si no, el dia saldria quemado.
    fija = A["exposicion"] is not None
    for prop in ("auto_exposure_min_brightness", "auto_exposure_max_brightness"):
        setp(s, "override_" + prop, fija, "PostProceso")
        if fija:
            setp(s, prop, A["exposicion"], "PostProceso")
    ppv.set_editor_property("settings", s)

    # ------------------------------------------------------------ jardin
    # De noche se dibujan las capas nocturnas (luciernagas).
    noche = AMBIENTE == "noche"
    for g in (a for a in actores if isinstance(a, unreal.JardinWorldActor)):
        if setp(g, "is_night", noche, "Jardin"):
            g.generate()

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)

    unreal.log("[Atmosfera] ======== Ambiente '%s'%s ========"
               % (AMBIENTE, " + niebla espesa" if niebla_espesa_activa() else ""))
    unreal.log("[Atmosfera] %d ajustes aplicados" % len(aplicados))
    for f in fallidos:
        unreal.log_warning("[Atmosfera]   NO APLICADO  " + f)
    unreal.log("[Atmosfera] %d aplicados, %d no aplicados" % (len(aplicados), len(fallidos)))


main()
