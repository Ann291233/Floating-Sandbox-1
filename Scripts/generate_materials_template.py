import json
from operator import itemgetter
import re
import sys



def main():
    
    if len(sys.argv) != 3 or (sys.argv[2] != "s" and sys.argv[2] != "e"):
        print("Usage: generate_materials_template.py <path_to_materials_json> s|e")
        sys.exit(-1)

    json_content = ""
    comment_re = re.compile(r"^(.*?)(//.+)?$")
    with open(sys.argv[1]) as f:
        for line in f:
            json_content += comment_re.sub(r"\1", line)

    data = json.loads(json_content)

    is_structural = (sys.argv[2] == "s")


    #
    # Prepare data
    #

    # d: key=row_order, val=list((row_name,col_order,col_name,Material))
    d={}
    
    for elem in data:
        
        row = elem["template"]["row"]
        row_parts = row.split("|")
        assert(len(row_parts) == 2)
        row_order = int(row_parts[0])
        row_name = row_parts[1]
            
        col = str(elem["template"]["column"])
        col_parts = col.split("|")
        col_name = None
        col_order = None
        if len(col_parts) == 2:
            col_name = col_parts[1]
            col_order = int(col_parts[0])
        else:
            assert(len(col_parts) == 1)
            col_name = ""
            col_order = int(col_parts[0])

        d_vals = d.setdefault(row_order, [])
        d_vals.append((row_name,col_order,col_name,elem))

    # Sort cols
    for k,v in d.items():
        d[k] = sorted(v, key=itemgetter(1))


    #
    # Generate HTML
    #

    html = "<html><head><title>Materials Template</title>"
    html += "<style>tr.border_top td { border-top:1pt solid black;}</style>"
    html += "</head><body>"

    if is_structural:
        spacing = 0
    else:
        spacing = 1

    html += "<table style='border: 1px solid black' cellpadding=0 cellspacing={}>".format(spacing)

    # Visit all rows
    for k in sorted(d.keys()):

        input_col_values = d[k]

        output_col_values = []

        # 1. Prepare data
        ci = 0
        labels_count = 0
        cur_row_name = None
        while ci < len(input_col_values):
            input_col_value = input_col_values[ci]
            if input_col_value[1] + labels_count <= len(output_col_values):
                if input_col_value[0] != cur_row_name:
                    output_col_values.append(input_col_value[0])
                    labels_count += 1
                    cur_row_name = input_col_value[0]
                else:
                    output_col_values.append(input_col_value)
                    ci += 1
            else:
                output_col_values.append(None)

        # 2. Render

        # Colors
        html += "<tr>"
        for c in output_col_values:
            if c:
                if type(c) != tuple:
                    # Title
                    html += "<td valign='middle' align='right' style='padding-right:5px;font-size:10px;'>" + c + "</td>"
                else:
                    html += "<td bgcolor='" + c[3]["color_key"] + "'class='border_top' style='width: 50px;'>&nbsp;</td>"
            else:
                html += "<td style='width: 50px;'>&nbsp;</td>"

        # Name
        html += "<tr>"
        for c in output_col_values:
            if c:
                if type(c) != tuple:
                    html += "<td/>"
                else:
                    html += "<td style='font-size:8px;'>" + c[2] + "</td>"
            else:
                html += "<td/>"
        html += "</tr>"

        # Data
        html += "<tr>"
        for c in output_col_values:
            if c:
                if type(c) != tuple:
                    html += "<td/>"
                else:
                    html += "<td style='font-size:8px;'>"
                    if is_structural:
                        html += "{:.2f}".format(c[3]["mass"]["nominal_mass"] * c[3]["mass"]["density"]) + "|" + str(c[3]["strength"]) + "|" + str(c[3]["stiffness"])
                    html += "</td>"
            else:
                html += "<td/>"
        html += "</tr>"

        
    html += "</table>";
    html += "</body></html>";

    with open("materials_template.html", "w") as html_file:
        html_file.write(html)

main()
