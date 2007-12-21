
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
   <tr><td>__Total clients:__</td>      <td> <TMPL_VAR nb_client> </td>
       <td>__Total bytes stored:__</td> <td> <TMPL_VAR nb_bytes> </td>
       <td>__Total media:__</td>        <td> <TMPL_VAR nb_media> </td>
   </tr>
   <tr><td>__Database size:__</td>      <td> <TMPL_VAR db_size> </td>
       <td>__Total Pool:__</td>         <td> <TMPL_VAR nb_pool> </td>
       <td>__Total Job:__</td>         <td> <TMPL_VAR nb_job> </td>
   </tr>
   <tr><td>__Job failed__ (<TMPL_VAR label>):</td> 
       
<td <TMPL_IF nb_err> class='joberr' </TMPL_IF>>
   <TMPL_VAR nb_err> 
</td>
       <td></td>         <td></td>
       <td></td>         <td></td>
   </tr>
  
  </table>
</div>
