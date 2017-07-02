<html>
<head>
    <link rel="stylesheet"
          href="/static/chartist.min.css">
    <script src="/static/chartist.min.js"></script>
</head>

<body>
<div class="ct-chart"></div>

<script>
% i=0
var data = {
    labels: [\\
        % for row in table:
            % if xaxis!="":
                {{row[xaxis]}}, \\
            % else:
                {{i}}, \\
            %end        
            % i+=1
        % end
        ],
    series: [[
        % for row in table:
            {{row[yaxis]}}, \\
        % end
        ]]
};

new Chartist.Line('.ct-chart', data);

</script>

</body>
</html>
