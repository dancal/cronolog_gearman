<?
$count = 0;
# create the gearman client
$worker = new GearmanWorker('10.128.5.11');
$worker->addServer();


$worker->addFunction("cronolog_svr", "servicegroup", &$count);
while ($worker->work());

function servicegroup($job)  {
    global $count;

    $workload = trim($job->workload());
    $lstemp  = explode("\n", $workload);

    foreach ( $lstemp as $val ) {
        $lsLine = explode(" ", $val);
        echo "== $lsLine[0] - $count - " . strlen($val). "\n";
        $count++;
    }

}

?>
