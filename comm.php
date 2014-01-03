<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">
<?php
$on_state = 1;
function init($socket)
{
    $d = microtime(true)."\n";
    $socket = socket_create(AF_UNIX, SOCK_STREAM, 0);

    if ($socket == false) {
        echo "socket_create() failed: reason: ".socket_strerror(socket_last_error()) . "\n";
        exit();
    }
    $res = socket_connect($socket, '/tmp/comm_socket');
    if ($res == false) {
        echo "socket_connect() failed: reason: ".socket_strerror(socket_last_error()) . "\n";
        exit();
    }
    return $socket;
}

function read_data($socket)
{
    global $on_state;

    $cmd = "read\n";
    $sent = socket_write($socket, $cmd);
    if ($sent === false) {
        echo "socket_write() failed: reason: ".socket_strerror(socket_last_error()) . "\n";
    }
    $cnt = socket_read($socket, 128, PHP_NORMAL_READ);
    echo "[\n";
    for ($i = 0; $i < $cnt; $i++) {
        $res = socket_read($socket, 26, PHP_NORMAL_READ);
        list($t, $v, $g) = sscanf($res, "%f %f %d");
        if ($i == 0) {
            $on_state = $g;
        }
        echo "{\n";
        echo "\"ax\": ".$i.",";
        echo "\"ay\": ".$t.",";
        echo "\"bx\": ".$i.",";
        echo "\"by\": ".$v;
        echo "}";
        if($i < $cnt -1) {
            echo ",\n";
        }
    }
    echo "];\n";
}


function on_heater($socket)
{
    $socket = init();
    socket_write($socket, "on\n");
    socket_close($socket);
}

function off_heater($socket)
{
    $socket = init();
    socket_write($socket, "off\n");
    socket_close($socket);
}

if (isset($_POST['OFF'])) {
    off_heater($socket);
}
else if (isset($_POST['ON']))
{
    on_heater($socket);
}

?>

<html>
  <head>
        <meta http-equiv="Content-Type" content="text/html; charset=utf-8">
        <title>Comm</title>
        <link rel="stylesheet" href="style.css" type="text/css">
        <script src="../amcharts/amcharts.js" type="text/javascript"></script>
        <script src="../amcharts/xy.js" type="text/javascript"></script>
        <script type="text/javascript">
            var chart;
            var graph;


            var chartData =
<?php
    $socket = init();

    $data = read_data($socket);
    socket_close($socket);
?>
             AmCharts.ready(function () {
                // XY CHART
                chart = new AmCharts.AmXYChart();
                chart.pathToImages = "../amcharts/images/";
                chart.dataProvider = chartData;
                chart.startDuration = 0;


                // AXES
                // X
                var xAxis = new AmCharts.ValueAxis();
                xAxis.title = "Readings  (older at the right)";
                xAxis.position = "bottom";
                xAxis.dashLength = 1;
                xAxis.axisAlpha = 0;
                xAxis.autoGridCount = true;
                chart.addValueAxis(xAxis);

                // Y
                var yAxis = new AmCharts.ValueAxis();
                yAxis.position = "left";
                yAxis.title = "Temperature";
                yAxis.dashLength = 1;
                yAxis.axisAlpha = 0;
                yAxis.autoGridCount = true;
                yAxis.axisThickness = 2;
                yAxis.minMaxMultiplier = 5;
                chart.addValueAxis(yAxis);

                var yAxis2 = new AmCharts.ValueAxis();
                yAxis2.position = "left";
                yAxis2.title = "Voltage" ;
                yAxis2.dashLength = 1;
                yAxis2.axisAlpha = 0;
                yAxis2.autoGridCount = true;
                yAxis2.minimum = 6;
                yAxis2.maximum = 15;
                yAxis2.offset = 50;

                chart.addValueAxis(yAxis2);

                // GRAPHS
                // triangles up
                var graph1 = new AmCharts.AmGraph();
                graph1.lineColor = "#FF6600";
                graph1.balloonText = "x:[[x]] y:[[y]]";
                graph1.xField = "ax";
                graph1.yField = "ay";
                graph1.lineAlpha = 10;
                //graph1.bullet = "round";
                graph1.yAxis = yAxis;
                chart.addGraph(graph1);

                // triangles down
                var graph2 = new AmCharts.AmGraph();
                graph2.lineColor = "#FCD202";
                graph2.balloonText = "x:[[x]] y:[[y]]";
                graph2.xField = "bx";
                graph2.yField = "by";
                graph2.lineAlpha = 3;
                //graph2.bullet = "round";
                graph2.yAxis = yAxis2;
                chart.addGraph(graph2);

                // CURSOR
                var chartCursor = new AmCharts.ChartCursor();
                chart.addChartCursor(chartCursor);

                // SCROLLBAR

                var chartScrollbar = new AmCharts.ChartScrollbar();
                chart.addChartScrollbar(chartScrollbar);

                // WRITE
                chart.write("chartdiv");
            });
            // this method is called when chart is first inited as we listen for "dataUpdated" event
            function zoomChart() {
                // different zoom methods can be used - zoomToIndexes, zoomToDates, zoomToCategoryValues
                chart.zoomToDates(new Date(1972, 0), new Date(1984, 0));
            }
        </script>
    </head>

    <body>
    <table border = 2>
    <tr>
    <td>
        <div id="chartdiv" style="width:700px; height:400px;"></div>
    </td>
    <td valign=top>
    <form action="<?=$_SERVER['PHP_SELF'];?>" method="post">
        <input type="submit" name="ON" value="ON"
        <?php if ($on_state == 1) {
            echo "style=\"width:200px; height:200px;font-size:80px;color:red\">";
        }
        else
        {
           echo "style=\"width:200px; height:200px;font-size:80px;color:blue;\">";
        }
        ?>
    </form>
    <form action="<?=$_SERVER['PHP_SELF'];?>" method="post">
        <input type="submit" name="OFF" value="OFF"
        <?php if ($on_state == 1) {
            echo "style=\"width:200px; height:200px;font-size:80px;color:red\">";
        }
        else
        {
           echo "style=\"width:200px; height:200px;font-size:80px;color:blue;\">";
        }
        ?>

    </form>
    </td>
    </tr>

    </table>

    <form name "mysubmit" id="mysubmit" action="comm.php"method="post">
        <script>setTimeout(function(){document.getElementById('mysubmit').submit()}, 5*1000);</script>
</form>
    </body>


</html>

