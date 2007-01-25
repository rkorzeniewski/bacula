
<script type="text/javascript" language="JavaScript">
bweb_add_refresh();
</script>
<div class='titlediv'>
  <h1 class="newstitle">
   Informaciones
  </h1>
</div>
<div class="bodydiv">
  <table>
   <tr><td>Cantidad Clientes:</td>  <td> <TMPL_VAR nb_client> </td>
       <td>Bytes Almacenados:</td> <td> <TMPL_VAR nb_bytes> </td>
       <td>Cantidad Medios:</td>        <td> <TMPL_VAR nb_media> </td>
   </tr>
   <tr><td>Tamaño Base de Datos:</td>      <td> <TMPL_VAR db_size> </td>
       <td>Cantidad Pools:</td>         <td> <TMPL_VAR nb_pool> </td>
       <td>Cantidad Jobs:</td>         <td> <TMPL_VAR nb_job> </td>
   </tr>
   <tr><td>Jobs fallados (<TMPL_VAR label>):</td> 
       
<td <TMPL_IF nb_err> class='joberr' </TMPL_IF>>
   <TMPL_VAR nb_err> 
</td>
       <td></td>         <td></td>
       <td></td>         <td></td>
   </tr>
  
  </table>
</div>
