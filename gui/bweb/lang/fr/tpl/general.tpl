
<script type="text/javascript" language="JavaScript">
bweb_add_refresh();
</script>
<div class='titlediv'>
  <h1 class="newstitle">
   Informations
  </h1>
</div>
<div class="bodydiv">
  <table>
   <tr><td>Nombre de client :</td>      <td> <TMPL_VAR nb_client> </td>
       <td>Total des sauvegardes :</td> <td> <TMPL_VAR nb_bytes> </td>
       <td>Nombre de media:</td>        <td> <TMPL_VAR nb_media> </td>
   </tr>
   <tr><td>Taille de la base :</td>      <td> <TMPL_VAR db_size> </td>
       <td>Nombre de Pool :</td>         <td> <TMPL_VAR nb_pool> </td>
       <td>Nombre de Job :</td>         <td> <TMPL_VAR nb_job> </td>
   </tr>
   <tr><td>Job en erreur (<TMPL_VAR label>):</td> 
       
<td <TMPL_IF nb_err> class='joberr' </TMPL_IF>>
   <TMPL_VAR nb_err> 
</td>
       <td></td>         <td></td>
       <td></td>         <td></td>
   </tr>
  
  </table>
</div>
