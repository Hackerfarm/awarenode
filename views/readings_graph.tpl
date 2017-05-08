<head>
    <script src="/static/plotly-latest.min.js"></script>
</head>

<div id="tester" style="width:600px;height:250px;"></div>
% i=0
<script>
    TESTER = document.getElementById('tester');
    Plotly.plot( TESTER, [{
    x: [\\
        % for row in table:
            % if xaxis!="":
                {{row[xaxis]}}, \\
            % else:
                {{i}}, \\
            %end        
            % i+=1
        % end
        ],
    y: [
        % for row in table:
            {{row[yaxis]}}, \\
        % end
        ] }], {
    margin: { t: 0 } } );
</script>
