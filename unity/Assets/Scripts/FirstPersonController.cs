using UnityEngine;
using Jardin.Generation;

/// <summary>
/// Exploración en primera persona: WASD, ratón para mirar, gravedad y límites.
///
/// Cumple el objetivo específico 4 del protocolo. Deliberadamente mínimo:
/// no hay salto doble, agacharse, inercia ni nada que no aporte al proyecto.
///
/// Usa CharacterController en lugar de Rigidbody porque para caminar sobre
/// terreno irregular da un control más predecible y no pelea con la física.
///
/// Requiere que el objeto tenga un CharacterController y una cámara como hijo.
/// </summary>
[RequireComponent(typeof(CharacterController))]
public class FirstPersonController : MonoBehaviour
{
    [Header("Movimiento")]
    public float walkSpeed = 6f;
    public float runSpeed = 11f;
    public float gravity = -20f;

    [Header("Cámara")]
    public Transform cameraPivot;
    public float mouseSensitivity = 2.2f;
    public float pitchLimit = 85f;

    [Header("Mundo")]
    public WorldBootstrap world;

    [Tooltip("Margen respecto al borde del mapa, en metros.")]
    public float boundsMargin = 1.5f;

    private CharacterController _controller;
    private float _pitch;
    private float _verticalSpeed;
    private bool _cursorLocked;

    private void Awake()
    {
        _controller = GetComponent<CharacterController>();

        if (cameraPivot == null && Camera.main != null)
            cameraPivot = Camera.main.transform;
    }

    private void Start()
    {
        LockCursor(true);
        SnapToTerrain();
    }

    private void Update()
    {
        // Escape libera el cursor; clic vuelve a capturarlo.
        // Sin esto, depurar dentro del editor es insoportable.
        if (Input.GetKeyDown(KeyCode.Escape)) LockCursor(false);
        if (Input.GetMouseButtonDown(0) && !_cursorLocked) LockCursor(true);

        if (_cursorLocked) Look();
        Move();
        ClampToBounds();
    }

    private void Look()
    {
        float yaw = Input.GetAxisRaw("Mouse X") * mouseSensitivity;
        float pitchDelta = Input.GetAxisRaw("Mouse Y") * mouseSensitivity;

        // El giro horizontal rota el cuerpo; el vertical solo la cámara.
        // Inclinar el cuerpo entero haría que el movimiento apuntara al cielo.
        transform.Rotate(0f, yaw, 0f);

        _pitch = Mathf.Clamp(_pitch - pitchDelta, -pitchLimit, pitchLimit);
        if (cameraPivot != null)
            cameraPivot.localRotation = Quaternion.Euler(_pitch, 0f, 0f);
    }

    private void Move()
    {
        float x = Input.GetAxisRaw("Horizontal");
        float z = Input.GetAxisRaw("Vertical");

        Vector3 input = new Vector3(x, 0f, z);
        // Normalizar evita que moverse en diagonal sea un 41% más rápido.
        if (input.sqrMagnitude > 1f) input.Normalize();

        float speed = Input.GetKey(KeyCode.LeftShift) ? runSpeed : walkSpeed;
        Vector3 move = transform.TransformDirection(input) * speed;

        if (_controller.isGrounded && _verticalSpeed < 0f)
            _verticalSpeed = -2f;   // pequeño empuje que mantiene isGrounded estable
        else
            _verticalSpeed += gravity * Time.deltaTime;

        move.y = _verticalSpeed;
        _controller.Move(move * Time.deltaTime);
    }

    /// <summary>
    /// Mantiene al jugador dentro del mapa delimitado, que es un compromiso
    /// explícito del protocolo.
    /// </summary>
    private void ClampToBounds()
    {
        var data = world != null ? world.World : null;
        if (data == null) return;

        float half = data.WorldSize * 0.5f - boundsMargin;
        Vector3 p = transform.position;

        float cx = Mathf.Clamp(p.x, -half, half);
        float cz = Mathf.Clamp(p.z, -half, half);

        if (cx != p.x || cz != p.z)
            transform.position = new Vector3(cx, p.y, cz);

        // Red de seguridad: si por algún motivo el jugador atraviesa la malla,
        // se le devuelve a la superficie en lugar de dejarlo caer al infinito.
        if (p.y < -50f) SnapToTerrain();
    }

    /// <summary>Coloca al jugador sobre el terreno en su posición horizontal actual.</summary>
    [ContextMenu("Colocar sobre el terreno")]
    public void SnapToTerrain()
    {
        var data = world != null ? world.World : null;
        if (data == null) return;

        Vector3 p = transform.position;
        float y = data.SampleHeightMeters(p.x, p.z) + _controller.height * 0.5f + 0.2f;

        // El CharacterController ignora cambios de transform mientras está activo.
        _controller.enabled = false;
        transform.position = new Vector3(p.x, y, p.z);
        _controller.enabled = true;

        _verticalSpeed = 0f;
    }

    private void LockCursor(bool locked)
    {
        _cursorLocked = locked;
        Cursor.lockState = locked ? CursorLockMode.Locked : CursorLockMode.None;
        Cursor.visible = !locked;
    }
}
