<br/>
<div class='titlediv'>
  <h1 class='newstitle'> Ayuda para cargar medios (part 1/2)</h1>
</div>
<div class="bodydiv">
Se seleccionará el mejor candidato para cargar. Se le pedirá
realizar su selección en la próxima pantalla.
  <form action="?" method='GET'>
   <table>
    <tr><td>Pool:</td>      
        <td><select name='pool' class='formulaire' multiple>
<TMPL_LOOP db_pools>
             <option selected><TMPL_VAR name></option>
</TMPL_LOOP>
           </select>
        </td>
    </tr>
    <tr><td>Tipo Medio:</td>      
        <td><select name='mediatype' class='formulaire' multiple>
<TMPL_LOOP db_mediatypes>
             <option><TMPL_VAR mediatype></option>
</TMPL_LOOP>
           </select>
        </td>
    </tr>
    <tr><td>
    Ubicación : 
    </td><td><select name='location' class='formulaire'>
  <TMPL_LOOP db_locations>
      <option value='<TMPL_VAR location>'><TMPL_VAR location></option>
  </TMPL_LOOP>
    </select>
    </td>
    </tr>
    <tr>
        <td>Expiración :</td> 
        <td> <input type='checkbox' name='expired' class='formulaire' 
                checked> </td>
    </tr>
    <tr>
        <td>Número de medio <br/> a cargar:</td> 
        <td> <input type='text' name='limit' class='formulaire' 
                size='3' value='10'> </td>
    </tr>
    <tr>
        <td><input type="image" name='action' value='compute_intern_media' 
                title='Siguiente' src='/bweb/next.png'>
        </td><td/>
    </tr>
   </table>
   </form>
</div>
