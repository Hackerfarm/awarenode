<table>
% if len(table)>0:
%   keys=table[0].keys()
% end
    <tr>
    % for k in keys:
        <td style="border:1px solid black">{{k}}</td>
    % end
    </tr>
% for row in table:
    <tr>
    % for k in keys:
        <td style="border:1px solid black">{{row[k]}}</td>
    % end
    </tr>
% end
</table>
