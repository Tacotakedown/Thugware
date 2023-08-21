public static Vector2 ? ObjectToScreen(Coordinates player, Coordinates actor)
{
    Tuple<double, double, double> player_camera = Tuple.Create((double)player.rot_x, (double)player.rot_y, (double)player.rot_z);
    List<List<double>> temp = MakeVMatrix(player_camera);

    Tuple<double, double, double> v_axis_x = Tuple.Create(temp[0][0], temp[0][1], temp[0][2]);
    Tuple<double, double, double> v_axis_y = Tuple.Create(temp[1][0], temp[1][1], temp[1][2]);
    Tuple<double, double, double> v_axis_z = Tuple.Create(temp[2][0], temp[2][1], temp[2][2]);

    Tuple<double, double, double> v_delta = Tuple.Create((double)actor.x - (double)player.x,
        (double)actor.y - (double)player.y,
        (double)actor.z - (double)player.z);
    List<double> v_transformed = new List<double>
    {
        Dot(v_delta, v_axis_y),
        Dot(v_delta, v_axis_z),
        Dot(v_delta, v_axis_x)
    };

    if (v_transformed[2] < 1.0) // No valid screen coordinates if its behind us
    {
        return null;
    }

    /* https://www.unknowncheats.me/forum/2348124-post130.html
     auto Ratio = SizeX / SizeY;
if (Ratio < 4.0f / 3.0f)
    Ratio = 4.0f / 3.0f;

auto FOV = Ratio / (16.0f / 9.0f) * tan(Camera.FOV * PI / 360.0f);

FVector Location;
Location.X = SizeX + Transform.X * SizeX / FOV / Transform.Z;
Location.Y = SizeY - Transform.Y * SizeX / FOV / Transform.Z;
             */

    double fov = player.fov; // horizontal FoV
    double screen_center_x = (double)SOT_WINDOW_W / 2;
    double screen_center_y = (double)SOT_WINDOW_H / 2;
    double aspect_ratio = (double)SOT_WINDOW_W / (double)SOT_WINDOW_H;

    if (aspect_ratio < 4.0f / 3.0f)
        aspect_ratio = 4.0f / 3.0f;

    var tmp_fov = aspect_ratio / (16.0f / 9.0f) * Math.Tan(fov * Math.PI / 360.0f);

    //double tmp_fov = Math.Tan(fov * Math.PI / 360);
    // Convert the horizontal FOV to vertical FOV
    //double h_fov = fov * Math.PI / 180; // Horizontal FoV in radians
    //double v_fov = 2 * Math.Atan(Math.Tan(h_fov / 2) / aspect_ratio); // Vertical FoV in radians

    //double x = (screen_center_x + (v_transformed[0] / v_transformed[2]) * screen_center_x / Math.Tan(h_fov / 2))*x_warp;
    //double y = (screen_center_y - (v_transformed[1] / v_transformed[2]) * screen_center_y / Math.Tan(v_fov / 2))*y_warp;

    double x = screen_center_x + (v_transformed[0] * (screen_center_x / tmp_fov) / v_transformed[2]);
    double y = screen_center_y - (v_transformed[1] * (screen_center_x / tmp_fov) / v_transformed[2]);

    if (x > SOT_WINDOW_W || x < 0 || y > SOT_WINDOW_H || y < 0)
    {
        return null;
    }

    return new Vector2((float)x, (float)y);
}